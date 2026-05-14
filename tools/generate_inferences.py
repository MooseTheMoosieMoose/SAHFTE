import json
import random
import math
import pathlib

#All the possible classes each modality can detect
classes = ["pedestrian", "vehicle", "traffic_cone"]
#A list of all the possible modalities
modalities = {
    "lidar" : {
        "detection_rate" : 0.95, #The % of all objects it sees
        "accuracy" : 0.6,  #The % of the time that an object is correctly identified
        "det_jitter" : 0.25, #the jitter between a detections origins and actual in meters
        "bb_jitter" : 0.1,  #The jitter between a bounding box's dimensions and detected in meters
    }, 
    "radar" : {
        "detection_rate" : 0.90,
        "accuracy" : 0.3,
        "det_jitter" : 0.4,
        "bb_jitter" : 0.3,
    }, 
    "camera" : {
        "detection_rate" : 1,
        "accuracy" : 0.7,
        "det_jitter" : 0.1,
        "bb_jitter" : 0.05
    }
}

#The max number of objects each modality will "see"
object_count_range = [100, 100]

#The distance from the vehicles path in which an object can spawn, in meters
object_distance_range = [2, 25]

#Number of seconds in each frame
frame_time_length = 0.1

#Meters per second speed of the vehicle
vehicle_speed = 11.176

METER_PER_DEG_LAT = 1 / 111111

class Coordinate:
    def __init__(self, lat=None, long=None, altitude=None):
        self.latitude = lat if lat is not None else random.uniform(-90, 90)
        self.longitude = long if long is not None else random.uniform(-180, 180)
        self.altitude = altitude if altitude is not None else random.uniform(-86, 8850)

    def clip(self):
            self.latitude = max(-90, min(90, self.latitude))
            # Use modulo to wrap correctly regardless of how large the number is
            self.longitude = (self.longitude + 180) % 360 - 180

    def jitter(self, planar_distance, altitude_shift):
        angle_of_travel = random.uniform(0, 2 * math.pi)
        
        end_lat = self.latitude + (math.cos(angle_of_travel) * planar_distance * METER_PER_DEG_LAT)
        
        # Initialize end_long with current longitude
        end_long = self.longitude
        
        # Only calculate longitude shift if we are not dangerously close to a pole
        # This prevents the "Pole Singularity" explosion
        if abs(self.latitude) < 89.9:
            # Removed "if lat != 0" check. cos(0) is 1, so this is safe at the equator.
            end_long += (math.sin(angle_of_travel) * planar_distance * METER_PER_DEG_LAT / math.cos(math.radians(self.latitude)))

        end_altitude = self.altitude + random.uniform(-1 * (altitude_shift / 2), (altitude_shift / 2))
        
        new_coord = Coordinate(end_lat, end_long, end_altitude)
        new_coord.clip()
        return new_coord
    
def sample_between(a: Coordinate, b: Coordinate, t: float = random.uniform(0, 1)) -> Coordinate:
    #Generate a new point in space, longitude has problems with wrapping on the date line
    sample_lat = a.latitude + (t * (b.latitude - a.latitude))

    #ensure that the longitude goes the right way
    long_diff = b.longitude - a.longitude
    if long_diff > 180:
        long_diff -= 360
    elif long_diff < -180:
        long_diff += 360

    #enforce that the final longitude value is still in the valid range
    sample_long = a.longitude + (t * long_diff)
    if sample_long > 180:
        sample_long -= 360
    elif sample_long < -180:
        sample_long += 360

    #Create a new random altitude
    sample_altitude = random.uniform(min(a.altitude, b.altitude), max(a.altitude, b.altitude))

    return Coordinate(sample_lat, sample_long, sample_altitude)

def main() -> None:
    #Create the start and stop points for the test
    start_pos = Coordinate()
    distance = frame_time_length * vehicle_speed
    end_pos = start_pos.jitter(distance, 1)

    #Create the current time
    frame_begin_time = random.uniform(1, 100)
    frame_end_time = frame_begin_time + frame_time_length

    #Create a place to store all our inferences and every object detected
    #inferences is a dictionary with keys being modality names for a list of inferences
    inferences = {}
    all_objects = []
    for modality_name in modalities:
        inferences[modality_name] = []

    #Simulate all the objects
    objects_in_sim = random.randint(object_count_range[0], object_count_range[1])
    for i in range(0, objects_in_sim):
        #Create an object to see which modalities will detect it
        obj_center = sample_between(start_pos, end_pos, (i + 1 / objects_in_sim))
        #TODO this assumes a general maximum object height spawn from the vehicles path to be 10 meters
        #TODO objects should also spawn with clusters, or even multiple detections per object
        obj_coord = obj_center.jitter(random.uniform(object_distance_range[0], object_distance_range[1]), 10)

        obj_dimensions = []
        for _ in range(0, 3):
            obj_dimensions.append(random.uniform(0.1, 1))

        obj_class = random.choice(classes)

        obj_time = frame_begin_time + (i * 1 / frame_time_length)

        #keep a record of all objects seen, the ground truth data
        all_objects.append({
            "timestamp" : obj_time,
            "class" : obj_class,
            "latitude" : obj_coord.latitude,
            "longitude" : obj_coord.longitude,
            "altitude" : obj_coord.altitude,
            "dimensions" : obj_dimensions,
            "obj_id" : i
        })

        #Give each modality a chance to see the new object
        for modality_name in modalities:
            modality_details = modalities[modality_name]

            #A modality "sees" an object if a random uniform variable is less than or equal to its accuracy
            observed_chance = random.uniform(0, 1)
            if (modality_details["detection_rate"] > observed_chance):

                #Once an object is detected by the modality we then have to see what it thinks the object is
                correct_chance = random.uniform(0, 1)
                det_class = random.choice(classes)
                if (modality_details["accuracy"] > correct_chance):
                    det_class = obj_class

                #A detection has some jitter on ground truth
                det_jitter = modality_details["det_jitter"]
                det_coord = obj_coord.jitter(det_jitter, det_jitter / 2)

                #and some jitter in sizing
                bb_jitter = modality_details["bb_jitter"] / 2
                det_dim = []
                for dim in obj_dimensions:
                    det_dim.append(dim + random.uniform(-1 *bb_jitter, bb_jitter))

                det_time = random.uniform(frame_begin_time, frame_end_time)

                new_inference = {}
                new_inference["timestamp"] = det_time
                new_inference["class"] = det_class
                new_inference["latitude"] = det_coord.latitude
                new_inference["longitude"] = det_coord.longitude
                new_inference["altitude"] = det_coord.altitude
                new_inference["dimensions"] = det_dim
                new_inference["obj_id"] = i
                inferences[modality_name].append(new_inference)
    
    #Now that we have everything lets log it
    print(f"Simulation complete, created {len(all_objects)} objects!")

    #Get the path to dump to
    store_folder_path = pathlib.Path(__file__).resolve().parent.parent / "test_data"
    print(f"Dump folder is set to: {store_folder_path}")

    #and dump to json
    for modality_name in inferences:
        file_path = store_folder_path / f"{modality_name}_sim_results.json"
        print(f"Dumping new modality simulation: {modality_name}")
        dump_dict = {"modality" : modality_name, "inferences" : inferences[modality_name]}
        with open(file_path, "w") as file:
            json.dump(dump_dict, file, indent=4)

    #And the ground truth
    print("Dumping ground truth")
    file_path = store_folder_path / f"gt_sim_results.json"
    ground_truth = {
        "start_pos" : [start_pos.latitude, start_pos.longitude, start_pos.altitude],
        "end_pos" : [end_pos.latitude, end_pos.longitude, end_pos.altitude],
        "distance" : distance,
        "simulated_duration" : frame_time_length,
        "modality" : "gt",
        "inferences" : all_objects
    }
    with open(file_path, "w") as file:
            json.dump(ground_truth, file, indent=4)

    #Finish
    print("All listed modalities simulated and dumped")



if __name__ == "__main__":
    main()
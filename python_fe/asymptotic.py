#Simple script that attempts to gauge the asymptotic behavior of the fusion system

from sahfte.sahfte import Fuser, FusionResult, Vec3D
import json, uuid, time, random, math

#Asymptotic Behavior Testing Config
OBJ_COUNTS = [10, 20, 50, 100, 500, 1000, 2000, 10000, 50000]
RUNS_PER_COUNT = 10

#Fuser Config
AUX_THREADS = 3
SPD = 8
BOUNDING_VOLUME = Vec3D(100.0, 100.0, 100.0)
REF_ORIGIN = Vec3D(-50.60755908581873, 165.9747292233417, 29.0)
REF_HEADING = 0
CLASSES = ["pedestrian", "cyclist", "car"]
MODS = {"camera" : 0.95, "lidar": 0.75, "radar": 0.6}

def main():
    for count in OBJ_COUNTS:
        print(f"Starting runs for {count} objects...")
        
        run_times = []
        for run_indx in range(0, RUNS_PER_COUNT, 1):
            print(f"-- Performing run index: {run_indx}")

            #Create the fuser
            fuser = Fuser(AUX_THREADS, SPD, BOUNDING_VOLUME, REF_ORIGIN, REF_HEADING)

            #Generate inferences, shuffle order for fairness
            objects_list = []
            for obj_id in range(0, count, 1):
                objects_list.extend(gen_inf_dicts(count))
            random.shuffle(objects_list)
            print(f"-- Total observations: {len(objects_list)}")

            #Shovel everything in
            for detection in objects_list:
                fuser.add_inference(
                    Vec3D(detection["pos"][0], detection["pos"][1], detection["pos"][2]), 
                    Vec3D(detection["dim"][0], detection["dim"][1], detection["dim"][2]), 
                    detection["rot"], 
                    detection["mod"], 
                    detection["class"], 
                    detection["conf"], 
                    str(uuid.uuid4()), 
                    False
                )
            
            #Time and fuse
            start = time.time()
            fuser.fuse(len(MODS))
            end = time.time()

            #Check on errs
            if not fuser.is_ok():
                print(f"Somethiong went wrong on object count: {count}, on run: {run_indx}, Err:: {fuser.get_error()}")

            #Get objects
            fused_obj = fuser.get_output_copy()
            # for obj in fused_obj:
            #     print(f"--- Object: {obj.uuid}")

            #Record timing
            run_length = (end - start) * 1000
            run_times.append(run_length)
            print(f"-- Run Time: {run_length} ms, with {len(fused_obj)} fused objects")
            if len(fused_obj) != count:
                print(f"-- Run dropped objects somewhere!")


        
        #Print the run results
        print(f"- Average Run Time with {count} objects: {sum(run_times) / len(run_times)}")

def gen_inf_dicts(objects_in_scene: int):
    #Store the inferences created of this object
    created_objects = []

    #Decide on the ground truth of this object
    rotation = random.uniform(0, 2 * math.pi)

    min_size = 0.1 / objects_in_scene
    max_size = 1.0 / objects_in_scene
    dimensions = [random.uniform(min_size, max_size) for _ in range(3)]

    position = [random.uniform(-50.0, 50.0) for _ in range(3)]

    class_name = random.choice(CLASSES)
    class_index = next((indx for indx, name in enumerate(CLASSES) if name == class_name), 0)

    #When generating a new item we must apply the object to each modality
    for mod in MODS:
        #An object is "seen" if its threshold is crossed
        if random.random() <= MODS[mod]:

            mod_indx = next((indx for indx, name in enumerate(MODS) if name == mod), 0)

            #Wiggle around the rotation, dimensions and position
            offset_position = [pos + random.uniform(-0.1 * min_size, 0.1 * min_size) for pos in position]
            offset_dimensions = [abs(dim + random.uniform(-0.1 * max_size, 0.1 * max_size)) for dim in dimensions]

            offset_rotation = rotation + random.uniform(-math.pi / 32, math.pi / 32) #Rotation +- 5ish degrees

            #See if the detected class is right
            offset_class = random.randint(0, len(CLASSES) - 1)
            if random.random() <= MODS[mod]:
                offset_class = class_index

            #Dump the jiggled inferences to the object
            created_objects.append({
                "pos" : offset_position,
                "dim" : offset_dimensions,
                "rot" : offset_rotation,
                "mod" : mod_indx,
                "class" : offset_class,
                "conf" : MODS[mod]
            })

    return created_objects

if __name__ == "__main__":
    main()
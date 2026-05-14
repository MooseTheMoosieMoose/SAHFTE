#Simple script that attempts to gauge the stability of the system through looping detecitons

from sahfte.sahfte import Fuser, FusionResult, Vec3D
import json, uuid, time, random, math

#Asymptotic Behavior Testing Config
OBJ_COUNT= 25
RUNS = 1000

#Fuser Config
AUX_THREADS = 3
SPD = 8
BOUNDING_VOLUME = Vec3D(100.0, 100.0, 100.0)
REF_ORIGIN = Vec3D(-50.60755908581873, 165.9747292233417, 29.0)
REF_HEADING = 0
CLASSES = ["pedestrian", "cyclist", "car"]
MODS = {"camera" : 0.95, "lidar": 0.75, "radar": 0.6}

def main():

    #Create the fuser
    fuser = Fuser(AUX_THREADS, SPD, BOUNDING_VOLUME, REF_ORIGIN, REF_HEADING)

    try:
        for loop_indx in range(0, RUNS, 1):
            #Generate inferences, shuffle order for fairness
            objects_list = []
            for obj_id in range(0, OBJ_COUNT, 1):
                objects_list.extend(gen_inf_dicts(OBJ_COUNT))
            random.shuffle(objects_list)

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
                print(f"Somethiong went wrong on run: {loop_indx}, Err:: {fuser.get_error()}")

            fused = fuser.get_output_copy()

            #Empty the buffers
            fuser.empty_buffers()
            print(f"Completed Run: {loop_indx}")
            
    except Exception as e:
        print(f"Something went wrong python wise: {e}")

    print("Test complete!")


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

import json, uuid, time

from sahfte import Fuser, Vec3D, FusionResult

modality_map = ["camera", "radar", "lidar"]
class_map = ["pedestrian", "vehicle", "traffic_cone"]

def main():
    #Create some consts that can be referenced elsewhere
    bounding_volume = Vec3D(100.0, 100.0, 100.0)
    gt_map = try_load_json(f"test_data/gt_sim_results.json")
    start_pos = Vec3D(gt_map["start_pos"][0], gt_map["start_pos"][1], gt_map["start_pos"][2])

    #Create the fuser
    fuser = Fuser(3, 8, bounding_volume, start_pos, 0.0)
    
    #Load all of our input data and shove it into the fuser
    total = 0
    for source in modality_map:
        new_data = try_load_json(f"test_data/{source}_sim_results.json")
        new_modality = new_data["modality"]
        new_inferences = new_data["inferences"]
        total += len(new_inferences)
        for detection in new_inferences:
            #Convert JSON to Vec3D
            det_pos = Vec3D(detection["latitude"], detection["longitude"], detection["altitude"])
            det_dim = Vec3D(detection["dimensions"][0], detection["dimensions"][1], detection["dimensions"][2])

            #Query what class and modality this belongs to
            det_mod = next((indx for indx, name in enumerate(modality_map) if name == new_modality), 0)
            det_class = next((indx for indx, name in enumerate(class_map) if name == detection["class"]), 0)

            #Shove it into the system
            fuser.add_inference(det_pos, det_dim, 0, det_mod, det_class, 0.5, str(uuid.uuid4()))

    #Fuse!
    print(f"Ready to fuse {total} items...")
    now = time.time()
    fuser.fuse(3)
    then = time.time()
    print(f"Fused! Took {(then - now) * 1000} Milliseconds")

    if not fuser.is_ok():
        print(f"Something went very wrong: {fuser.get_error()}")
    
    fused = fuser.get_output_copy()
    print(f"Produced {len(fused)} Items!")
    for item in fused:
        fused_class = class_map[item.class_name]
        uuids = item.uuid.split(":")
        print(f"Items joined: {len(uuids)}")

    fuser.empty_buffers()

def try_load_json(fp: str) -> dict:
    try:
        with open(fp, "r") as file:
            data = json.load(file)
            return data
    except FileNotFoundError:
        print(f"Failed to load file: {fp}")
    except json.JSONDecodeError:
        print(f"Selected file contains illegal JSON: {fp}")
    except Exception as e:
        print(f"Something went wrong while loading: {fp}, {e}")

if __name__ == "__main__":
    main()
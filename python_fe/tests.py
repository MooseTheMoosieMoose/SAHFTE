
import json

from sahfte import Fuser, Vec3D, FusionResult

import uuid

input_sources = ["camera", "radar", "lidar"]

def main():
    bounding_volume = Vec3D(100.0, 100.0, 100.0)
    fuser = Fuser(3, 8, bounding_volume, Vec3D(57.097689595459485, -8.803518178702546, 2192.3872210358936), 0.0)
    
    #Load all of our input data and shove it into the fuser
    total = 0
    for source in input_sources:
        new_data = try_load_json(f"test_data/{source}_sim_results.json")
        new_modality = new_data["modality"]
        new_inferences = new_data["inferences"]
        total += len(new_inferences)
        for detection in new_inferences:
            det_pos = Vec3D(detection["latitude"], detection["longitude"], detection["altitude"])
            det_dim = Vec3D(detection["dimensions"][0], detection["dimensions"][1], detection["dimensions"][2])
            fuser.add_inference(det_pos, det_dim, 0, new_modality, detection["class"], 0.5, str(uuid.uuid4()))

    #Fuse!
    print(f"Ready to fuse {total} items...")
    fuser.fuse(1)
    print("Fused!")

    if not fuser.is_ok():
        print(f"Something went very wrong: {fuser.get_error()}")
    
    for item in fuser.get_output_copy():
        print("\n\n")
        print(f"Fusion Results:\n\t \
              Class Name: {item.class_name}\n\t \
              Confidence: {item.confidence}\n\t \
              Modality String: {item.modality}\n\t \
              UUID String: [{item.uuid}]")

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
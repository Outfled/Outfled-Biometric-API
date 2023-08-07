import face_recognition
import pickle
import os
from pathlib import Path
from collections import Counter

def _get_face_encodings(image_path, model_type):
    image = face_recognition.load_image_file(image_path)
    face_locations = face_recognition.face_locations(image, model=model_type)
    return face_recognition.face_encodings(image, face_locations)

def create_known_face(image1_path, image2_path, image3_path, encoding_output, face_name, model_type = "dnn"):
    names = []
    encodings = []


    # Load the image files
    face_encodings = _get_face_encodings(image1_path, model_type)
    for encoding in face_encodings:
        encodings.append(encoding)

    if os.path.exists(image2_path):
        face_encodings = _get_face_encodings(image2_path, model_type)
        for encoding in face_encodings:
           encodings.append(encoding)
    
    if os.path.exists(image3_path):
        face_encodings = _get_face_encodings(image3_path, model_type)
        for encoding in face_encodings:
            encodings.append(encoding)
    
    name_encodings = {"names": face_name, "encodings": encodings}
    with Path(encoding_output).open(mode="wb") as f:
        pickle.dump(name_encodings, f)
    
def validate_face(image_path, encoding_path, model_type = "dnn"):
    
    with Path(encoding_path).open(mode="rb") as f:
        loaded_encodings = pickle.load(f)
        
    input_image = face_recognition.load_image_file(image_path)

    input_face_locations = face_recognition.face_locations(
        input_image, model=model_type
    )
    input_face_encodings = face_recognition.face_encodings(
        input_image, input_face_locations
    )
    
    for bounding_box, unknown_encoding in zip(
        input_face_locations, input_face_encodings
    ):
        name = _recognize_face(unknown_encoding, loaded_encodings)
        if name:
            return True
            
    return False
    
def _recognize_face(unknown_encoding, loaded_encodings):
    boolean_matches = face_recognition.compare_faces(
        loaded_encodings["encodings"], unknown_encoding
    )
    votes = Counter(
        name
        for match, name in zip(boolean_matches, loaded_encodings["names"])
        if match
    )
    if votes:
        return votes.most_common(1)[0][0]


    
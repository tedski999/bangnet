import time
import json
from database_handler import DatabaseHandler
from chat_bot import send_to_telegram
from django.views.decorators.csrf import csrf_exempt
from django.core.files.storage import default_storage
from django.core.files.base import ContentFile
from django.shortcuts import render
from datetime import datetime
from drf_yasg.utils import swagger_auto_schema
from drf_yasg import openapi
from rest_framework.decorators import api_view
from pathlib import Path
import os
from django.conf import settings
from django.http import HttpResponse, JsonResponse


# # Build paths inside the project like this: BASE_DIR / 'subdir'.
from cal_bang_location import get_environment_conditions, TDoALocalization, calculate_bearing
#
BASE_DIR = Path(__file__).resolve().parent.parent


class Address:
    def __init__(self, id, latitude, longitude):
        self.id = id
        self.latitude = latitude
        self.longitude = longitude


micros_addresses = [
    Address(0, 46.8822611, 12.88957897),
    Address(1, 46.8898053, 12.86917107),
    Address(2, 46.8868912, 12.87791352),
    Address(3, 14.8822611, 32.88957897),
    Address(4, 13.8822611, 31.88957897),
    Address(5, 11.8822611, 30.88957897),
    Address(10086, 10.8822611, 29.88957897)
]
camera_addresses = [
    Address(0, 46.8822612, 12.88957899),
    Address(1, 13.8822611, 31.88957897)
]


def get_address_by_id(address_list, id):
    for address in address_list:
        if address.id == id:
            return address
    return None


def hello(request):
    data = {'message': 'Hello, welcome to the API!'}
    return JsonResponse(data)


def index(request):
    media_path = os.path.join(settings.BASE_DIR, 'media', 'uploads')
    image_files = os.listdir(media_path)
    image_data = []

    for filename in image_files:
        image_url = f'/media/uploads/{filename}'
        current_time = datetime.now()
        timestamp = current_time.strftime("%Y-%m-%d %H:%M:%S")
        explosion_coord = "Get explosion coordinates from somewhere"  # Replace with actual coordinates
        image_data.append((image_url, timestamp, explosion_coord))

    return render(request, 'index.html', {'image_data': image_data})


@csrf_exempt
def handle_upload(request):
    if request.method == 'POST' and request.FILES['file']:
        uploaded_file = request.FILES['file']
        # 保存文件到指定路径
        with open('media/uploads/' + uploaded_file.name, 'wb+') as destination:
            for chunk in uploaded_file.chunks():
                destination.write(chunk)

        return render(request, 'index.html', {'uploaded_file': uploaded_file})

    return render(request, 'index.html', {'error': 'No file found in the request'})


@csrf_exempt
@swagger_auto_schema(
    methods=['GET'],
    request_body=openapi.Schema(
        type=openapi.TYPE_OBJECT,
        required=['micro_number', 'bang_time'],
        properties={
            # 'latitude': openapi.Schema(type=openapi.TYPE_NUMBER),
            # 'longitude': openapi.Schema(type=openapi.TYPE_NUMBER),
            'micro_number': openapi.Schema(type=openapi.TYPE_NUMBER),
            'bang_time': openapi.Schema(type=openapi.TYPE_NUMBER),
        },
    ),
    responses={200: 'Micro location uploaded successfully', 400: 'Bad Request'}
)
@api_view(['GET'])
def micro_record(request):
    try:
        db_handler = DatabaseHandler('bangnet_cloud/bangnet.db')
        #json_data = json.loads(request.body.decode('utf-8'))
        request_data = request.GET

        micro_number = int(request_data.get("micro_number"))
        bang_time = request_data.get("bang_time")

        if None in [micro_number, bang_time]:
            raise KeyError("One or more parameters are missing")

        db_handler.insert_data('micros', (None, micro_number, bang_time))

        response = {
            'statusCode': 200,
            'body': json.dumps({'message': f'Micro location uploaded successfully'}),
        }
        occur_bang()
    except KeyError as e:
        response = {
            'statusCode': 400,
            'request': json.dumps({'error': f'Missing {str(e)} parameter'}),
        }

    return JsonResponse(response)


@csrf_exempt
@swagger_auto_schema(
    methods=['GET'],
    request_body=openapi.Schema(
        type=openapi.TYPE_OBJECT,
        required=['camera_id'],
        properties={
            'camera_id': openapi.Schema(type=openapi.TYPE_NUMBER)
        },
    ),
    responses={200: 'Get camera angle successfully', 400: 'Bad Request'}
)
@api_view(['GET'])
def get_camera_angle(request):
    try:
        db_handler = DatabaseHandler('bangnet_cloud/bangnet.db')
        request_data = request.GET
        camera_id = int(request_data.get('camera_id'))
        result = db_handler.select_data('camera', columns=['id', 'angle'])
        target_angle = -1
        for row in result:
            if row[0] == camera_id:
                target_angle = row[1]
                break

        if target_angle != -1:
            return HttpResponse(target_angle)
        else:
            return HttpResponse(status=404)  # Or any appropriate response

    except KeyError as e:
        return JsonResponse({'error': f'Missing {str(e)} parameter'}, status=400)


@csrf_exempt
@swagger_auto_schema(
    methods=['POST'],
    request_body=openapi.Schema(
        type=openapi.TYPE_OBJECT,
        required=['file'],
        properties={
            'file': openapi.Schema(type=openapi.TYPE_FILE),
        },
    ),
    responses={200: 'File uploaded successfully', 400: 'Bad Request'}
)
@api_view(['POST'])
def upload_image(request):
    try:
        db_handler = DatabaseHandler('bangnet_cloud/bangnet.db')
        if request.method == 'POST' and request.FILES['file']:
            uploaded_file = request.FILES['file']
            file_path = default_storage.save('uploads/' + uploaded_file.name, ContentFile(uploaded_file.read()))
            db_handler.insert_data('bang_image', (None, file_path, time.time()))

            return JsonResponse({'message': 'File uploaded successfully'})
        else:
            return JsonResponse({'error': 'No file found in the request'})
    except KeyError as e:
        response = {
            'statusCode': 400,
            'body': json.dumps({'error': f'Missing {str(e)} parameter'}),
        }

    return JsonResponse(response)


def occur_bang():
    try:
        db_handler = DatabaseHandler('bangnet_cloud/bangnet.db')
        micros_bang_time_result = get_micros_bang_time(db_handler)
        micros_upload_times = len(micros_bang_time_result)
        if micros_upload_times < 3:
            print(f"occur_bang: micros_upload times < 3")
            return
        mics_coordinates = get_micros_address(micros_upload_times)
        print(f"mics_coordinates: {mics_coordinates}")
        print(f"micros_bang_time_result: {micros_bang_time_result}")
        temperature, humidity = get_environment_conditions()
        # initial_guess = [initial_x, initial_y]
        delta_t_measured = micros_bang_time_result
        localizer = TDoALocalization(mics_coordinates, temperature, humidity)
        estimated_source_position = localizer.localize(delta_t_measured)
        print("Estimated source position:", estimated_source_position)
        camera_address = get_camera_addresses(0)
        camera_lat = camera_address.latitude
        camera_lon = camera_address.longitude
        explosion_bearing = calculate_bearing(camera_lat, camera_lon, estimated_source_position[0],
                                              estimated_source_position[1])
        db_handler.insert_data('camera', (None, explosion_bearing))
        print("Explosion bearing:", explosion_bearing)
        db_handler.delete_all_data('micros')
        send_to_telegram(
            f"micros_bang_time_result: {micros_bang_time_result}\n"
            f"mics_coordinates: {mics_coordinates}\n"
            f'Estimated source position: {estimated_source_position}\n'
            f'Explosion bearing: {explosion_bearing}')
    except Exception as e:
        print(f"cal occur_bang error: {e}")


def get_micros_bang_time(db_handler):
    contain_id = set()
    target_result = []
    result = db_handler.select_data('micros', columns=['id', 'micro_number', 'bang_time'])
    for row in result:
        if row[1] not in contain_id:
            target_result.append(row[2])
            contain_id.add(row[1])
    return target_result


def get_micros_address(mirco_nums):
    #result = db_handler.select_data('micros', columns=['id', 'micro_number', 'bang_time'])
    # for row in result:
    #     micro_number = row[1]
    #     micro_address = get_address_by_id(micros_addresses, int(micro_number))
    #     latitude = micro_address.latitude
    #     longitude = micro_address.longitude
    #     micros_address.append((latitude, longitude))
    micros_address_result = []
    for i in range(0, mirco_nums):
        micro_address = get_address_by_id(micros_addresses, i)
        latitude = micro_address.latitude
        longitude = micro_address.longitude
        micros_address_result.append((latitude, longitude))
    return micros_address_result


def get_camera_addresses(camera_id):
    return get_address_by_id(camera_addresses, camera_id)
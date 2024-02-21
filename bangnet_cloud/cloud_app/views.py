import time
import os
from django.http import JsonResponse
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
from rest_framework.response import Response
from rest_framework.decorators import api_view, permission_classes, parser_classes
from rest_framework.permissions import AllowAny
from rest_framework.parsers import MultiPartParser


def hello(request):
    data = {'message': 'Hello, welcome to the API!'}
    return JsonResponse(data)


def index(request):
    image_files = os.listdir('media/uploads')
    image_data = []
    for filename in image_files:
        image_url = '/media/uploads/' + filename
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
    methods=['POST'],
    request_body=openapi.Schema(
        type=openapi.TYPE_OBJECT,
        required=['latitude', 'longitude', 'micro_number', 'bang_time'],
        properties={
            'latitude': openapi.Schema(type=openapi.TYPE_NUMBER),
            'longitude': openapi.Schema(type=openapi.TYPE_NUMBER),
            'micro_number': openapi.Schema(type=openapi.TYPE_STRING),
            'bang_time': openapi.Schema(type=openapi.TYPE_STRING, format=openapi.FORMAT_DATETIME),
        },
    ),
    responses={200: 'Micro location uploaded successfully', 400: 'Bad Request'}
)
@api_view(['POST'])
def micro_record(request):
    try:
        db_handler = DatabaseHandler('bangnet_cloud/bangnet.db')
        json_data = json.loads(request.body.decode('utf-8'))

        latitude = json_data.get("latitude")
        longitude = json_data.get("longitude")
        micro_number = json_data.get("micro_number")
        bang_time = json_data.get("bang_time")

        if None in [latitude, longitude, micro_number, bang_time]:
            raise KeyError("One or more parameters are missing")

        db_handler.insert_data('micros', (None, latitude, longitude, micro_number, bang_time))

        response = {
            'statusCode': 200,
            'body': json.dumps({'message': f'Micro location uploaded successfully'}),
        }
        send_to_telegram(f'Micro location uploaded successfully: \n'
                         f'latitude: {latitude} \n longitude: {longitude} \n micro_number: {micro_number} \n bang_time: {bang_time}')
    except KeyError as e:
        response = {
            'statusCode': 400,
            'body': json.dumps({'error': f'Missing {str(e)} parameter'}),
        }

    return JsonResponse(response)


@csrf_exempt
def get_camera_angle(request):
    try:
        db_handler = DatabaseHandler('bangnet_cloud/bangnet.db')

        response = {
            'statusCode': 200,
            'body': json.dumps({'message': f'Micro location uploaded successfully'}),
        }
    except KeyError as e:
        response = {
            'statusCode': 400,
            'body': json.dumps({'error': f'Missing {str(e)} parameter'}),
        }

    return JsonResponse(response)


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


@csrf_exempt
def occur_bang(request):
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


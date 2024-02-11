from django.http import JsonResponse
import json
from database_handler import DatabaseHandler
from chat_bot import send_to_telegram
from django.views.decorators.csrf import csrf_exempt


def hello(request):
    data = {'message': 'Hello, welcome to the API!'}
    return JsonResponse(data)


@csrf_exempt
def micro_record(request):
    try:
        db_handler = DatabaseHandler('bangnet.db')
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
        send_to_telegram(f'Micro location uploaded successfully')
    except KeyError as e:
        response = {
            'statusCode': 400,
            'body': json.dumps({'error': f'Missing {str(e)} parameter'}),
        }

    return JsonResponse(response)


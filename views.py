from django.http import JsonResponse, HttpResponse
from django.views.decorators.csrf import csrf_exempt
import json

def home(request):
    return HttpResponse("Welcome to the Bus Management API")

@csrf_exempt
def test_post_data(request):
    if request.method == 'POST':
        try:
            data = json.loads(request.body)
            print("Received data:", data)  # Print data to console
            return JsonResponse({'status': 'success', 'data': data}, status=200)
        except json.JSONDecodeError:
            return JsonResponse({'status': 'error', 'message': 'Invalid JSON'}, status=400)
    return JsonResponse({'status': 'error', 'message': 'Invalid request method'}, status=405)
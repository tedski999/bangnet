from django.urls import path
from .views import micro_record

urlpatterns = [
    path('micro_record/', micro_record, name='micro_record'),
]

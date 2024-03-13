from django.urls import path
from .views import micro_record, upload_image, get_camera_angle, handle_upload, index
from django.conf import settings
from django.conf.urls.static import static

urlpatterns = [
    path('', index, name='index'),
    path('micro_record/', micro_record, name='micro_record'),
    path('upload_image/', upload_image, name='upload_image'),
    #path('occur_bang/', occur_bang, name='occur_bang'),
    path('get_camera_angle/', get_camera_angle, name='get_camera_angle'),
    path('handle_upload/', handle_upload, name='handle_upload'),
]

if settings.DEBUG:
    urlpatterns += static(settings.MEDIA_URL, document_root=settings.MEDIA_ROOT)
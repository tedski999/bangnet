from database_handler import DatabaseHandler
import time

db_handler = DatabaseHandler('bangnet.db')

#db_handler.create_table('micros', ['id INTEGER PRIMARY KEY AUTOINCREMENT', 'latitude DOUBLE', 'longitude DOUBLE', 'micro_number INTEGER',
                                  #'bang_time TIMESTAMP'])

#db_handler.create_table('camera', ['id INTEGER PRIMARY KEY AUTOINCREMENT', 'angle_x DOUBLE', 'angle_y DOUBLE'])
#db_handler.create_table('bang_image', ['id INTEGER PRIMARY KEY AUTOINCREMENT', 'image_url TEXT', 'upload_time TIMESTAMP'])

#db_handler.insert_data('micros', (None, 66.343, 44.3242, 1, time.time()))

#db_handler.update_data('micros', {'latitude': 44.232}, 'micro_number = 1')


#db_handler.delete_data('micros', 'micro_number = 0')

if db_handler.check_empty_table('bang_image') == -1:
    print("Table is null")
else:
    print("Table is not null")

#db_handler.update_data('micros', {'micro_number': 1}, 'latitude': 44.232')

# result = db_handler.select_data('micros', columns=['latitude', 'longitude', 'micro_number', 'bang_time'])
# for row in result:
#     print("latitude:", row[0])
#     print("longitude:", row[1])
#     print("micro_number:", row[2])
#     print("bang_time:", row[3])
#     print("-----------")
result = db_handler.select_data('bang_image', columns=['image_url', 'upload_time'])
for row in result:
    print("image_url:", row[0])
    print("upload_time:", row[1])
    print("-----------")

db_handler.close_connection()

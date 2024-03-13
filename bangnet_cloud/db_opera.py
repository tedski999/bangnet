from database_handler import DatabaseHandler
import time

db_handler = DatabaseHandler('bangnet.db')

#db_handler.create_table('micros', ['id INTEGER PRIMARY KEY AUTOINCREMENT', 'latitude DOUBLE', 'longitude DOUBLE', 'micro_number INTEGER',
                                  #'bang_time TIMESTAMP'])
#db_handler.create_table('micros', ['id INTEGER PRIMARY KEY AUTOINCREMENT', 'micro_number UNSIGNED BIGINT', 'bang_time UNSIGNED BIGINT'])

#db_handler.create_table('camera', ['id INTEGER PRIMARY KEY AUTOINCREMENT', 'angle UNSIGNED BIGINT'])
#db_handler.create_table('bang_image', ['id INTEGER PRIMARY KEY AUTOINCREMENT', 'image_url TEXT', 'upload_time TIMESTAMP'])
#db_handler.create_table('bang_incident', ['id INTEGER PRIMARY KEY AUTOINCREMENT', 'latitude DOUBLE', 'longitude DOUBLE', 'image_url TEXT', 'bang_time UNSIGNED BIGINT'])

#db_handler.insert_data('micros', (None, 3, time.time()))
#db_handler.insert_data('camera', (None, 90))
#db_handler.insert_data('bang_incident', (None, 8.9234, 5.567665, "http://img.jpg", "233423"))


#db_handler.update_data('micros', {'latitude': 44.232}, 'micro_number = 1')


#db_handler.delete_data('micros', 'micro_number = 0')
#db_handler.delete_all_data('micros')

if db_handler.check_empty_table('camera') == -1:
    print("Table is null")
else:
    print("Table is not null")

#db_handler.update_data('micros', {'micro_number': 1}, 'latitude': 44.232')

# result = db_handler.select_data('micros', columns=['id', 'micro_number', 'bang_time'])
# for row in result:
#     print("id:", row[0])
#     print("micro_number:", row[1])
#     print("bang_time:", row[2])
#     print("-----------")
# result = db_handler.select_data('bang_image', columns=['image_url', 'upload_time'])
# for row in result:
#     print("image_url:", row[0])
#     print("upload_time:", row[1])
#     print("-----------")
result = db_handler.select_data('camera', columns=['id', 'angle'])
for row in result:
    print("id:", row[0])
    print("angle:", row[1])
    print("-----------")
# result = db_handler.select_data('bang_incident', columns=['id', 'latitude', 'longitude', 'image_url', 'bang_time'])
# for row in result:
#     print("id:", row[0])
#     print("latitude:", row[1])
#     print("longitude:", row[2])
#     print("image_url:", row[3])
#     print("bang_time:", row[4])
#     print("-----------")

# contain_id = set()
# target_result = []
# result = db_handler.select_data('micros', columns=['id', 'micro_number', 'bang_time'])
# for row in result:
#     if row[1] not in contain_id:
#         target_result.append(row[2])
#         contain_id.add(row[1])
# print(target_result)



db_handler.close_connection()

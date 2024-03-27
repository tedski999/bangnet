import sqlite3


class DatabaseHandler:
    def __init__(self, db_file):
        self.conn = sqlite3.connect(db_file)
        self.cursor = self.conn.cursor()

    def create_table(self, table_name, columns):
        columns_str = ', '.join(columns)
        query = f"CREATE TABLE IF NOT EXISTS {table_name} ({columns_str})"
        self.cursor.execute(query)
        self.conn.commit()

    def insert_data(self, table_name, data):
        placeholders = ', '.join(['?' for _ in data])
        query = f"INSERT INTO {table_name} VALUES ({placeholders})"
        self.cursor.execute(query, data)
        self.conn.commit()

    def update_data(self, table_name, set_values, condition):
        set_clause = ', '.join([f"{key} = ?" for key in set_values.keys()])
        query = f"UPDATE {table_name} SET {set_clause} WHERE {condition}"
        self.cursor.execute(query, tuple(set_values.values()))
        self.conn.commit()

    def delete_data(self, table_name, condition):
        query = f"DELETE FROM {table_name} WHERE {condition}"
        self.cursor.execute(query)
        self.conn.commit()

    def delete_all_data(self, table_name):
        query = f"DELETE FROM {table_name}"
        self.cursor.execute(query)
        self.conn.commit()

    def select_data(self, table_name, columns=None, condition=None, order_by=None):
        columns_str = ', '.join(columns) if columns else '*'
        query = f"SELECT {columns_str} FROM {table_name}"
        if condition:
            query += f" WHERE {condition}"
        if order_by:
            query += f" ORDER BY {order_by} DESC"
        self.cursor.execute(query)
        return self.cursor.fetchall()

    def check_empty_table(self, table_name):
        self.cursor.execute(f"SELECT COUNT(*) FROM {table_name}")
        result = self.cursor.fetchone()[0]
        return -1 if result == 0 else 0

    def close_connection(self):
        self.cursor.close()
        self.conn.close()
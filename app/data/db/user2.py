from ittopjournal import get_token, get_user_info
import sqlite3

token = get_token("Shesh_hz44", "7W0Lc21t", "6a56a5df2667e65aab73ce76d1dd737f7d1faef9c52e8b8c55ac75f565d8e8a6")

user = get_user_info(token)
coins = user["gaming_points"][0]["points"]
gems = user["gaming_points"][1]["points"]
name = user["full_name"]

conn = sqlite3.connect('main.db')
cursor = conn.cursor()

def create_database_grade():
    cursor.execute('''
    CREATE TABLE IF NOT EXISTS user_info (
        id INTEGER PRIMARY KEY AUTOINCREMENT,
        name TEXT NOT NULL, 
        coins INTEGER NOT NULL,
        gems INTEGER NOT NULL
    )
    ''')

    conn.commit()
    print("База данных и таблица успешно созданы!")


def add_user_info(cursor, conn, name, coins, gems):
    cursor.execute('''
    INSERT INTO user_info (name, coins, gems)
    VALUES (?, ?, ?)
    ''', (name, coins, gems))
    conn.commit()
    print(f"Добавлено: {name} - {coins}к - {gems}г.")

create_database_grade()
add_user_info(cursor, conn, name, coins, gems)
conn.close()
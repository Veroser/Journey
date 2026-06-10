from ittopjournal import get_token, get_homework
import sqlite3
token = get_token("Shesh_hz44", "7W0Lc21t", "6a56a5df2667e65aab73ce76d1dd737f7d1faef9c52e8b8c55ac75f565d8e8a6")

conn = sqlite3.connect('homework.db')
cursor = conn.cursor()

homework = get_homework(token)

def create_table():
    cursor.execute('''
        CREATE TABLE IF NOT EXISTS tasks (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            checked INTEGER DEFAULT 0,
            current INTEGER DEFAULT 0,
            overdue INTEGER DEFAULT 0,
            under_inspection INTEGER DEFAULT 0,
            deleted INTEGER DEFAULT 0,
            all_tasks INTEGER DEFAULT 0
        )
    ''')
    conn.commit()

def insert_task(checked, current, overdue, under_inspection, deleted, all_tasks):
    cursor.execute('''
        INSERT INTO tasks (checked, current, overdue, under_inspection, deleted, all_tasks)
        VALUES (?, ?, ?, ?, ?, ?)
    ''', (checked, current, overdue, under_inspection, deleted, all_tasks))
    conn.commit()
    print(f"Запись добавлена.")

insert_task(homework[0]["counter"], homework[1]["counter"], homework[2]["counter"], homework[3]["counter"], homework[4]["counter"], homework[5]["counter"])
from ittopjournal import get_token, get_feedback_info
import sqlite3
token = get_token("Shesh_hz44", "7W0Lc21t", "6a56a5df2667e65aab73ce76d1dd737f7d1faef9c52e8b8c55ac75f565d8e8a6")

conn = sqlite3.connect('feedback.db')
cursor = conn.cursor()

def create_database_grade():
    cursor.execute('''
    CREATE TABLE IF NOT EXISTS performance (
        id INTEGER PRIMARY KEY AUTOINCREMENT,
        date TEXT NOT NULL, 
        message TEXT NOT NULL,
        full_spec TEXT NOT NULL,
        teacher TEXT NOT NULL
    )
    ''')
    conn.commit()
    print("База данных и таблица успешно созданы!")

def add_feedback(cursor, conn, date, message, full_spec, teacher):
    cursor.execute('''
    INSERT INTO performance (date, message, full_spec, teacher)
    VALUES (?, ?, ?, ?)
    ''', (date, message, full_spec, teacher))
    conn.commit()
    print(f"Добавлено: {date} - {message} - {full_spec} - {teacher}.")

def parse_feedback():
    for i in range(len(feedback)):
        add_feedback(cursor, conn, feedback[i]["date"], feedback[i]["message"], 
                    feedback[i]["full_spec"], feedback[i]["teacher"])

# ВАЖНО: Сначала создаем таблицу
create_database_grade()

# Получаем данные
feedback = get_feedback_info(token)

# Добавляем данные
if feedback:
    parse_feedback()
    print("Все данные успешно добавлены!")
else:
    print("Нет данных для добавления")

conn.close()
print("Соединение с БД закрыто")
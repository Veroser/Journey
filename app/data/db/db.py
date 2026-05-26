import sqlite3
import calendar
from datetime import datetime
from ittopjournal import get_schedule, get_token
token = get_token("Shesh_hz44", "7W0Lc21t", "6a56a5df2667e65aab73ce76d1dd737f7d1faef9c52e8b8c55ac75f565d8e8a6")
now = datetime.now()
last_day = calendar.monthrange(now.year, now.month)[1]

db = sqlite3.connect("schedule.db")
c = db.cursor()
def parse_schedule():
    for i in range(1, last_day+1):
        if i < 10:
            schedule = get_schedule(token, f"{now.year}-{now.month}-0{i}")
        else:
            schedule = get_schedule(token, f"{now.year}-{now.month}-{i}")
        try:
            c.execute("INSERT OR IGNORE INTO study_days (date) VALUES (?)", (schedule[0]["date"],))
            c.execute("SELECT id FROM study_days WHERE date = ?", (schedule[0]["date"],))
            study_day_id = c.fetchone()[0]
            c.execute("""
                INSERT INTO lessons (study_day_id, date, lesson, started_at, finished_at, teacher_name, subject_name, room_name) 
                VALUES (?, ?, ?, ?, ?, ?, ?, ?)
            """, (
                study_day_id,
                schedule[0]["date"],
                schedule[0]["lesson"],
                schedule[0]["started_at"],
                schedule[0]["finished_at"],
                schedule[0]["teacher_name"],
                schedule[0]["subject_name"],
                schedule[0]["room_name"]
            ))
        except Exception: continue
parse_schedule()

"""{'date': '2026-05-25', 
'lesson': 1, 
'started_at': '08:00', 
'finished_at': '09:20', 
'teacher_name': 'Концевич Андрей Алексеевич', 
'subject_name': 'Объектно-ориентированное программирование с использованием языка C++', 
'room_name': '205 Adobe'}"""
db.close()
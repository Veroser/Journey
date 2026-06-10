#!/usr/bin/env python3
"""
Парсер расписания для журнала колледжа
Запускается из Qt приложения с JWT токеном
"""

import sys
import argparse
import sqlite3
import calendar
from datetime import datetime, timedelta
import warnings
import urllib3
import sys
import io

sys.stdout = io.TextIOWrapper(sys.stdout.buffer, encoding='utf-8')
sys.stderr = io.TextIOWrapper(sys.stderr.buffer, encoding='utf-8')

warnings.filterwarnings("ignore")
urllib3.disable_warnings()

from ittopjournal import get_schedule

def parse_period(token: str, db_path: str, start_date: str, end_date: str = None):
    """
    Парсит расписание за указанный период
    """
    
    # Подключаемся к БД
    db = sqlite3.connect(db_path)
    c = db.cursor()
    
    # Парсим даты
    start = datetime.strptime(start_date, "%Y-%m-%d")
    if end_date:
        end = datetime.strptime(end_date, "%Y-%m-%d")
    else:
        end = start
    
    # 🧹 ОЧИЩАЕМ ВСЕ ДАННЫЕ ПЕРЕД ПАРСИНГОМ
    print("🧹 Очистка старых данных...")
    sys.stdout.flush()
    
    # Удаляем все уроки
    c.execute("DELETE FROM lessons")
    deleted_lessons = c.rowcount
    print(f"🗑️ Удалено уроков: {deleted_lessons}")
    
    # Удаляем все дни
    c.execute("DELETE FROM study_days")
    deleted_days = c.rowcount
    print(f"🗑️ Удалено дней: {deleted_days}")
    
    # Сохраняем изменения
    db.commit()
    print("✅ Старые данные удалены")
    sys.stdout.flush()
    
    # Считаем общее количество дней для прогресса
    total_days = (end - start).days + 1
    processed_days = 0
    
    print(f"🔄 Начало парсинга с {start_date} по {end_date}")
    print(f"📅 Всего дней: {total_days}")
    sys.stdout.flush()
    
    current_date = start
    while current_date <= end:
        date_str = current_date.strftime("%Y-%m-%d")
        
        try:
            # Получаем расписание на день
            schedule = get_schedule(token, date_str)
            
            if schedule and len(schedule) > 0:
                # Вставляем день в study_days
                c.execute("INSERT OR IGNORE INTO study_days (date) VALUES (?)", (date_str,))
                c.execute("SELECT id FROM study_days WHERE date = ?", (date_str,))
                result = c.fetchone()
                
                if result:
                    study_day_id = result[0]
                    
                    # Вставляем все уроки за день
                    for lesson in schedule:
                        if isinstance(lesson, dict):
                            c.execute("""
                                INSERT OR IGNORE INTO lessons 
                                (study_day_id, date, lesson, started_at, finished_at, 
                                 teacher_name, subject_name, room_name) 
                                VALUES (?, ?, ?, ?, ?, ?, ?, ?)
                            """, (
                                study_day_id,
                                lesson.get("date", date_str),
                                lesson.get("lesson", 0),
                                lesson.get("started_at", ""),
                                lesson.get("finished_at", ""),
                                lesson.get("teacher_name", ""),
                                lesson.get("subject_name", ""),
                                lesson.get("room_name", "")
                            ))
                    
                    print(f"✅ {date_str}: {len(schedule)} уроков")
                else:
                    print(f"⚠️ {date_str}: ошибка сохранения дня")
            else:
                print(f"📭 {date_str}: нет занятий")
                
        except Exception as e:
            print(f"❌ {date_str}: ошибка - {str(e)}")
        
        # Прогресс
        processed_days += 1
        percent = int((processed_days / total_days) * 100)
        print(f"PROGRESS:{percent}")
        sys.stdout.flush()
        
        current_date += timedelta(days=1)
    
    # Сохраняем изменения
    db.commit()
    db.close()
    
    print("✅ Парсинг завершен!")
    sys.stdout.flush()


def main():
    parser = argparse.ArgumentParser(description='Парсер расписания журнала')
    parser.add_argument('--token', required=True, help='JWT токен авторизации')
    parser.add_argument('--db', required=True, help='Путь к файлу БД')
    parser.add_argument('--start-date', help='Начальная дата (YYYY-MM-DD)')
    parser.add_argument('--end-date', help='Конечная дата (YYYY-MM-DD)')
    
    args = parser.parse_args()
    
    if not args.start_date:
        # По умолчанию: текущий месяц ± 1 месяц
        now = datetime.now()
        
        # Предыдущий месяц
        prev_month = now.replace(day=1) - timedelta(days=1)
        start_date = prev_month.replace(day=1).strftime("%Y-%m-%d")
        
        # Следующий месяц
        if now.month == 12:
            next_month = now.replace(year=now.year+1, month=1, day=1)
        else:
            next_month = now.replace(month=now.month+1, day=1)
        last_day = calendar.monthrange(next_month.year, next_month.month)[1]
        end_date = next_month.replace(day=last_day).strftime("%Y-%m-%d")
        
        print(f"📅 Период по умолчанию: {start_date} - {end_date}")
    else:
        start_date = args.start_date
        end_date = args.end_date if args.end_date else start_date
    
    try:
        parse_period(args.token, args.db, start_date, end_date)
        sys.exit(0)
    except Exception as e:
        print(f"❌ Критическая ошибка: {str(e)}", file=sys.stderr)
        sys.exit(1)


if __name__ == "__main__":
    main()
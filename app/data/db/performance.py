#!/usr/bin/env python3
import sys
import os
import argparse
import sqlite3

# Добавляем текущую папку в путь
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

from ittopjournal import get_token, get_metric_grade_info, get_metric_attendance_info


def parse_grades(token: str, db_path: str):
    db = sqlite3.connect(db_path)
    c = db.cursor()

    c.execute('''
        CREATE TABLE IF NOT EXISTS grade (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            date TEXT NOT NULL, 
            points INTEGER NOT NULL
        )
    ''')

    c.execute("DELETE FROM grade")

    try:
        grade_metric = get_metric_grade_info(token)

        if not grade_metric:
            print("Нет данных об оценках")
            return

        for item in grade_metric:
            c.execute('''
                INSERT INTO grade (date, points)
                VALUES (?, ?)
            ''', (item["date"], item["points"]))

        db.commit()
        print(f"Оценки: Добавлено {len(grade_metric)} записей")

    except Exception as e:
        print(f"Ошибка парсинга оценок: {str(e)}")
        db.rollback()
        raise
    finally:
        db.close()


def parse_attendance(token: str, db_path: str):
    """Парсинг посещаемости"""
    db = sqlite3.connect(db_path)
    c = db.cursor()

    c.execute('''
        CREATE TABLE IF NOT EXISTS attendance (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            date TEXT NOT NULL, 
            points INTEGER NOT NULL
        )
    ''')

    c.execute("DELETE FROM attendance")

    try:
        attendance_metric = get_metric_attendance_info(token)

        if not attendance_metric:
            print("Нет данных о посещаемости")
            return

        for item in attendance_metric:
            c.execute('''
                INSERT INTO attendance (date, points)
                VALUES (?, ?)
            ''', (item["date"], item["points"]))

        db.commit()
        print(f"Посещаемость: Добавлено {len(attendance_metric)} записей")

    except Exception as e:
        print(f"Ошибка парсинга посещаемости: {str(e)}")
        db.rollback()
        raise
    finally:
        db.close()


def parse_metrics(token: str, db_path: str):
    #Парсинг всех метрик (оценки + посещаемость)
    parse_grades(token, db_path)
    parse_attendance(token, db_path)


def main():
    parser = argparse.ArgumentParser(description='Парсер метрик (оценки и посещаемость)')
    parser.add_argument('--token', required=True, help='JWT токен авторизации')
    parser.add_argument('--db', required=True, help='Путь к файлу БД metrics.db')
    parser.add_argument('--type', choices=['grades', 'attendance', 'all'], 
                       default='all', help='Тип метрик для парсинга (по умолчанию: all)')

    args = parser.parse_args()

    try:
        if args.type == 'grades':
            parse_grades(args.token, args.db)
        elif args.type == 'attendance':
            parse_attendance(args.token, args.db)
        else:
            parse_metrics(args.token, args.db)
        
        sys.exit(0)
    except Exception as e:
        print(f"Критическая ошибка: {str(e)}", file=sys.stderr)
        sys.exit(1)


if __name__ == "__main__":
    main()
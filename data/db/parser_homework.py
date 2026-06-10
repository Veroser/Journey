#!/usr/bin/env python3
import sys
import os
import argparse
import sqlite3

# Добавляем текущую папку (db/) в путь — ittopjournal рядом
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

from ittopjournal import get_homework


def parse_homework(token: str, db_path: str):
    db = sqlite3.connect(db_path)
    c = db.cursor()

    c.execute('''
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

    c.execute("DELETE FROM tasks")

    try:
        homework = get_homework(token)

        checked = homework[0]["counter"]
        current = homework[1]["counter"]
        overdue = homework[2]["counter"]
        under_inspection = homework[3]["counter"]
        deleted = homework[4]["counter"]
        all_tasks = homework[5]["counter"]

        c.execute('''
            INSERT INTO tasks (checked, current, overdue, under_inspection, deleted, all_tasks)
            VALUES (?, ?, ?, ?, ?, ?)
        ''', (checked, current, overdue, under_inspection, deleted, all_tasks))

        db.commit()
        print(f"✅ ДЗ: Всего={all_tasks}, Сдано={checked}, На проверке={under_inspection}, Просрочено={overdue}, Текущие={current}")

    except Exception as e:
        print(f"❌ Ошибка парсинга ДЗ: {str(e)}")
        db.rollback()
        raise
    finally:
        db.close()


def main():
    parser = argparse.ArgumentParser(description='Парсер домашних заданий')
    parser.add_argument('--token', required=True, help='JWT токен авторизации')
    parser.add_argument('--db', required=True, help='Путь к файлу БД homework.db')

    args = parser.parse_args()

    try:
        parse_homework(args.token, args.db)
        sys.exit(0)
    except Exception as e:
        print(f"❌ Критическая ошибка: {str(e)}", file=sys.stderr)
        sys.exit(1)


if __name__ == "__main__":
    main()
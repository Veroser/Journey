#!/usr/bin/env python3
import sys
import os
import argparse
import sqlite3

# Добавляем текущую папку в путь
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

from ittopjournal import get_rating_group_info, get_rating_stream_info, get_user_info


def parse_rating(token: str, db_path: str):
    """Парсинг рейтинга (группа + поток)"""
    db = sqlite3.connect(db_path)
    c = db.cursor()

    # Таблица для позиции пользователя
    c.execute('''
        CREATE TABLE IF NOT EXISTS user_rate (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            all_group_amount INTEGER NOT NULL,
            all_stream_amount INTEGER NOT NULL,
            group_user_position INTEGER NOT NULL,
            stream_user_position INTEGER NOT NULL
        )
    ''')

    # Таблица для рейтинга группы
    c.execute('''
        CREATE TABLE IF NOT EXISTS group_rate (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            amount INTEGER NOT NULL,
            full_name TEXT NOT NULL,
            position INTEGER NOT NULL
        )
    ''')

    # Таблица для рейтинга потока
    c.execute('''
        CREATE TABLE IF NOT EXISTS stream_rate (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            amount INTEGER NOT NULL,
            full_name TEXT NOT NULL,
            position INTEGER NOT NULL
        )
    ''')

    # Очищаем старые данные
    c.execute("DELETE FROM user_rate")
    c.execute("DELETE FROM group_rate")
    c.execute("DELETE FROM stream_rate")

    try:
        # Получаем данные через API
        rating = get_rating_group_info(token)
        stream_rating = get_rating_stream_info(token)
        user_info = get_user_info(token)
        real_student_id = user_info["student_id"] if user_info else None

        if not rating:
            print("Нет данных о рейтинге группы")
            return
        if not stream_rating:
            print("Нет данных о рейтинге потока")
            return

        # Парсим рейтинг группы
        for item in rating:
            c.execute('''
                INSERT INTO group_rate (amount, full_name, position)
                VALUES (?, ?, ?)
            ''', (item.get("amount", 0), item.get("full_name", ""), item.get("position", 0)))
            print(f"Группа: {item.get('full_name')} - позиция {item.get('position')} - {item.get('amount')} баллов")

        # Парсим рейтинг потока
        for item in stream_rating:
            amount = item.get("amount", 0) if item.get("amount") is not None else 0
            full_name = item.get("full_name", "null") if item.get("full_name") else "null"
            c.execute('''
                INSERT INTO stream_rate (amount, full_name, position)
                VALUES (?, ?, ?)
            ''', (amount, full_name, item.get("position", 0)))
            print(f"Поток: {full_name} - позиция {item.get('position')} - {amount} баллов")

        # Вычисляем позицию пользователя
        all_group_amount = len(rating)
        group_user_position = 0
        for i, item in enumerate(rating):
            if real_student_id and real_student_id == item.get("id"):
                group_user_position = i + 1
                break

        all_stream_amount = stream_rating[-1]["position"] if stream_rating else 0
        stream_user_position = 0
        for item in stream_rating:
            if real_student_id and real_student_id == item.get("id"):
                stream_user_position = item.get("position", 0)
                break

        # Сохраняем позицию пользователя
        c.execute('''
            INSERT INTO user_rate (all_group_amount, all_stream_amount, group_user_position, stream_user_position)
            VALUES (?, ?, ?, ?)
        ''', (all_group_amount, all_stream_amount, group_user_position, stream_user_position))

        db.commit()
        print(f"Рейтинг: группа {group_user_position}/{all_group_amount}, поток {stream_user_position}/{all_stream_amount}")

    except Exception as e:
        print(f"Ошибка парсинга рейтинга: {str(e)}")
        db.rollback()
        raise
    finally:
        db.close()


def main():
    parser = argparse.ArgumentParser(description='Парсер рейтинга (группа и поток)')
    parser.add_argument('--token', required=True, help='JWT токен авторизации')
    parser.add_argument('--db', required=True, help='Путь к файлу БД main.db')

    args = parser.parse_args()

    try:
        parse_rating(args.token, args.db)
        sys.exit(0)
    except Exception as e:
        print(f"Критическая ошибка: {str(e)}", file=sys.stderr)
        sys.exit(1)


if __name__ == "__main__":
    main()
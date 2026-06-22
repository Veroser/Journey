#!/usr/bin/env python3
import sys
import os
import argparse
import sqlite3

# Добавляем текущую папку в путь
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

from ittopjournal import get_user_info


def parse_user_info(token: str, db_path: str):
    """Парсинг информации о пользователе (монеты, геммы, имя)"""
    db = sqlite3.connect(db_path)
    c = db.cursor()

    c.execute('''
        CREATE TABLE IF NOT EXISTS user_info (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            name TEXT NOT NULL, 
            coins INTEGER NOT NULL,
            gems INTEGER NOT NULL
        )
    ''')

    c.execute("DELETE FROM user_info")

    try:
        user = get_user_info(token)

        if not user:
            print("Нет данных о пользователе")
            return

        name = user.get("full_name", "")
        coins = 0
        gems = 0

        gaming_points = user.get("gaming_points", [])
        if len(gaming_points) > 0:
            coins = gaming_points[0].get("points", 0)
        if len(gaming_points) > 1:
            gems = gaming_points[1].get("points", 0)

        c.execute('''
            INSERT INTO user_info (name, coins, gems)
            VALUES (?, ?, ?)
        ''', (name, coins, gems))

        db.commit()
        print(f"Пользователь: {name} - {coins} монет, {gems} геммов")

    except Exception as e:
        print(f"Ошибка парсинга пользователя: {str(e)}")
        db.rollback()
        raise
    finally:
        db.close()


def main():
    parser = argparse.ArgumentParser(description='Парсер информации о пользователе')
    parser.add_argument('--token', required=True, help='JWT токен авторизации')
    parser.add_argument('--db', required=True, help='Путь к файлу БД main.db')

    args = parser.parse_args()

    try:
        parse_user_info(args.token, args.db)
        sys.exit(0)
    except Exception as e:
        print(f"Критическая ошибка: {str(e)}", file=sys.stderr)
        sys.exit(1)


if __name__ == "__main__":
    main()
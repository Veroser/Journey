from ittopjournal import get_token, get_rating_group_info, get_rating_stream_info, get_user_info
import sqlite3


token = get_token("Shesh_hz44", "7W0Lc21t", "6a56a5df2667e65aab73ce76d1dd737f7d1faef9c52e8b8c55ac75f565d8e8a6")
rating = get_rating_group_info(token)
stream_rating = get_rating_stream_info(token)
real_student_id = get_user_info(token)["student_id"]

conn = sqlite3.connect('main.db')
cursor = conn.cursor()



def add_user_rate(all_group_amount, all_stream_amount, group_user_position, stream_user_position):
    cursor.execute('''
        INSERT INTO user_rate (all_group_amount, all_stream_amount, group_user_position, stream_user_position)
        VALUES (?, ?, ?, ?)
        ''', (all_group_amount, all_stream_amount, group_user_position, stream_user_position))
    conn.commit()

def add_student(cursor, conn, amount, full_name, position):
    cursor.execute('''
    INSERT INTO group_rate (amount, full_name, position)
    VALUES (?, ?, ?)
    ''', (amount, full_name, position))
    conn.commit()

    print(f"Добавлен ученик: {full_name} - {position} - {amount}.")

def parse_group_rating():
    for i in range(len(rating)):
        add_student(cursor, conn, rating[i]["amount"], rating[i]["full_name"], rating[i]["position"])

def add_student_stream(cursor, conn, amount, full_name, position):
    cursor.execute('''
    INSERT INTO stream_rate (amount, full_name, position)
    VALUES (?, ?, ?)
    ''', (amount, full_name, position))
    conn.commit()

    print(f"Добавлен ученик: {full_name} - {position} - {amount}.")

def parse_stream_rating():
    for i in range(len(stream_rating)):
        if stream_rating[i]["amount"] is not None:
            add_student_stream(cursor, conn, stream_rating[i]["amount"], stream_rating[i]["full_name"], stream_rating[i]["position"])
        else:
            add_student_stream(cursor, conn, 0, "null",stream_rating[i]["position"])

def group_position():
    all_group_amount = len(rating)
    user_position = 0

    for i in range(all_group_amount):
        if real_student_id == rating[i]["id"]:
            user_position = i+1

    return all_group_amount, user_position

def stream_position():
    all_amount = len(stream_rating)
    all_stream_amount = stream_rating[all_amount-1]["position"]
    user_position = 0

    for i in range(all_amount):
        if real_student_id == stream_rating[i]["id"]:
            user_position = stream_rating[i]["position"]

    return all_stream_amount, user_position

add_user_rate(group_position()[0], stream_position()[0], group_position()[1], stream_position()[1])
conn.close()


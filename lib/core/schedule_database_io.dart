import 'package:path/path.dart' as path;
import 'package:sqflite/sqflite.dart';

import '../models/lesson.dart';
import 'dates.dart';

class ScheduleDatabase {
  Database? _database;

  Future<Database> _open() async {
    final existing = _database;
    if (existing != null) {
      return existing;
    }
    final base = await getDatabasesPath();
    final database = await openDatabase(
      path.join(base, 'journey.db'),
      version: 1,
      onCreate: (db, version) async {
        await db.execute('''
          CREATE TABLE lessons (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            date TEXT NOT NULL,
            lesson_number INTEGER NOT NULL,
            start_time TEXT NOT NULL,
            end_time TEXT NOT NULL,
            teacher_name TEXT NOT NULL,
            subject_name TEXT NOT NULL,
            room_name TEXT NOT NULL,
            search_text TEXT NOT NULL
          )
        ''');
        await db.execute('CREATE INDEX idx_lessons_date ON lessons(date)');
      },
    );
    _database = database;
    return database;
  }

  Future<void> replaceMonth(DateTime month, List<Lesson> lessons) async {
    final database = await _open();
    final start = dateKey(DateTime(month.year, month.month, 1));
    final end = dateKey(DateTime(month.year, month.month + 1, 0));
    await database.transaction((txn) async {
      await txn.delete(
        'lessons',
        where: 'date >= ? AND date <= ?',
        whereArgs: [start, end],
      );
      final batch = txn.batch();
      for (final lesson in lessons) {
        batch.insert('lessons', lesson.toDatabase());
      }
      await batch.commit(noResult: true);
    });
  }

  Future<List<Lesson>> lessonsBetween(DateTime start, DateTime end) async {
    final database = await _open();
    final rows = await database.query(
      'lessons',
      where: 'date >= ? AND date <= ?',
      whereArgs: [dateKey(start), dateKey(end)],
      orderBy: 'date ASC, start_time ASC, lesson_number ASC',
    );
    return rows.map(Lesson.fromDatabase).toList();
  }

  Future<List<Lesson>> search(String query) async {
    final normalized = query.trim().toLowerCase();
    if (normalized.isEmpty) {
      return [];
    }
    final database = await _open();
    final rows = await database.query(
      'lessons',
      where: 'instr(search_text, ?) > 0',
      whereArgs: [normalized],
      orderBy: 'date ASC, start_time ASC, lesson_number ASC',
      limit: 500,
    );
    return rows.map(Lesson.fromDatabase).toList();
  }

  Future<List<Lesson>> lessonsForNotification(DateTime from, DateTime to) async {
    final database = await _open();
    final rows = await database.query(
      'lessons',
      where: 'date >= ? AND date <= ?',
      whereArgs: [dateKey(from), dateKey(to)],
      orderBy: 'date ASC, start_time ASC, lesson_number ASC',
    );
    return rows.map(Lesson.fromDatabase).toList();
  }

  Future<void> close() async {
    final database = _database;
    _database = null;
    await database?.close();
  }
}

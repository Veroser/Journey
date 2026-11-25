import 'dart:convert';

import 'package:shared_preferences/shared_preferences.dart';

import '../models/lesson.dart';
import 'dates.dart';

/// Web-реализация хранилища расписания.
///
/// sqflite не поддерживается в браузере, поэтому занятия сохраняются
/// в localStorage (через shared_preferences) в виде JSON.
class ScheduleDatabase {
  static const String _key = 'schedule_lessons_v1';

  List<Map<String, Object?>>? _rows;
  int _nextId = 1;

  Future<List<Map<String, Object?>>> _load() async {
    final cached = _rows;
    if (cached != null) {
      return cached;
    }
    final prefs = await SharedPreferences.getInstance();
    final raw = prefs.getString(_key);
    final rows = <Map<String, Object?>>[];
    if (raw != null && raw.isNotEmpty) {
      try {
        final decoded = jsonDecode(raw);
        if (decoded is List) {
          for (final item in decoded) {
            if (item is Map) {
              rows.add(Map<String, Object?>.from(item));
            }
          }
        }
      } on FormatException {
        rows.clear();
      }
    }
    for (final row in rows) {
      final id = row['id'];
      if (id is int && id >= _nextId) {
        _nextId = id + 1;
      }
    }
    _rows = rows;
    return rows;
  }

  Future<void> _persist() async {
    final prefs = await SharedPreferences.getInstance();
    await prefs.setString(_key, jsonEncode(_rows ?? const <Object?>[]));
  }

  Future<void> replaceMonth(DateTime month, List<Lesson> lessons) async {
    final rows = await _load();
    final start = dateKey(DateTime(month.year, month.month, 1));
    final end = dateKey(DateTime(month.year, month.month + 1, 0));
    rows.removeWhere((row) {
      final date = row['date'] as String? ?? '';
      return date.compareTo(start) >= 0 && date.compareTo(end) <= 0;
    });
    for (final lesson in lessons) {
      final map = lesson.toDatabase();
      map['id'] = _nextId++;
      rows.add(map);
    }
    await _persist();
  }

  Future<List<Lesson>> lessonsBetween(DateTime start, DateTime end) async {
    final rows = await _load();
    final from = dateKey(start);
    final to = dateKey(end);
    final result = rows
        .where((row) {
          final date = row['date'] as String? ?? '';
          return date.compareTo(from) >= 0 && date.compareTo(to) <= 0;
        })
        .map(Lesson.fromDatabase)
        .toList();
    result.sort(_compare);
    return result;
  }

  Future<List<Lesson>> search(String query) async {
    final normalized = query.trim().toLowerCase();
    if (normalized.isEmpty) {
      return [];
    }
    final rows = await _load();
    final result = rows
        .where((row) {
          final text = row['search_text'] as String? ?? '';
          return text.contains(normalized);
        })
        .map(Lesson.fromDatabase)
        .take(500)
        .toList();
    result.sort(_compare);
    return result;
  }

  Future<List<Lesson>> lessonsForNotification(DateTime from, DateTime to) =>
      lessonsBetween(from, to);

  Future<void> close() async {
    _rows = null;
  }

  int _compare(Lesson a, Lesson b) {
    final byDate = a.date.compareTo(b.date);
    if (byDate != 0) {
      return byDate;
    }
    final byTime = a.startTime.compareTo(b.startTime);
    if (byTime != 0) {
      return byTime;
    }
    return a.number.compareTo(b.number);
  }
}

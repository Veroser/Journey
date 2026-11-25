import '../core/dates.dart';

class Lesson {
  const Lesson({
    this.id,
    required this.date,
    required this.number,
    required this.startTime,
    required this.endTime,
    required this.teacher,
    required this.subject,
    required this.room,
  });

  final int? id;
  final DateTime date;
  final int number;
  final String startTime;
  final String endTime;
  final String teacher;
  final String subject;
  final String room;

  static Lesson? fromApi(Map<String, dynamic> json) {
    final date = parseDate(json['date']?.toString() ?? '');
    if (date == null) {
      return null;
    }
    final subject = json['subject_name']?.toString().trim() ?? '';
    if (subject.isEmpty) {
      return null;
    }
    return Lesson(
      date: date,
      number: _asInt(json['lesson']),
      startTime: normalizeTime(json['started_at']?.toString() ?? ''),
      endTime: normalizeTime(json['finished_at']?.toString() ?? ''),
      teacher: json['teacher_name']?.toString().trim() ?? '',
      subject: subject,
      room: json['room_name']?.toString().trim() ?? '',
    );
  }

  static Lesson fromDatabase(Map<String, Object?> row) {
    return Lesson(
      id: row['id'] as int?,
      date: parseDate(row['date'] as String? ?? '') ?? DateTime(1970),
      number: (row['lesson_number'] as int?) ?? 0,
      startTime: row['start_time'] as String? ?? '',
      endTime: row['end_time'] as String? ?? '',
      teacher: row['teacher_name'] as String? ?? '',
      subject: row['subject_name'] as String? ?? '',
      room: row['room_name'] as String? ?? '',
    );
  }

  Map<String, Object?> toDatabase() {
    return {
      'date': dateKey(date),
      'lesson_number': number,
      'start_time': startTime,
      'end_time': endTime,
      'teacher_name': teacher,
      'subject_name': subject,
      'room_name': room,
      'search_text': '$subject $teacher $room'.toLowerCase(),
    };
  }

  static int _asInt(Object? value) {
    if (value is int) {
      return value;
    }
    return int.tryParse(value?.toString() ?? '') ?? 0;
  }
}

import 'package:flutter/foundation.dart' show kIsWeb;
import 'package:flutter_local_notifications/flutter_local_notifications.dart';
import 'package:flutter_timezone/flutter_timezone.dart';
import 'package:timezone/data/latest.dart' as tz;
import 'package:timezone/timezone.dart' as tz;

import '../models/lesson.dart';

class NotificationService {
  NotificationService._();

  static final NotificationService instance = NotificationService._();

  final FlutterLocalNotificationsPlugin _plugin =
      FlutterLocalNotificationsPlugin();

  bool _ready = false;

  Future<void> initialize() async {
    if (kIsWeb) {
      // В браузере локальные уведомления не используются.
      _ready = false;
      return;
    }
    tz.initializeTimeZones();
    try {
      final zone = await FlutterTimezone.getLocalTimezone();
      tz.setLocalLocation(tz.getLocation(zone.identifier));
    } on Exception {
      tz.setLocalLocation(tz.getLocation('Etc/UTC'));
    }
    try {
      await _plugin.initialize(
        settings: const InitializationSettings(
          android: AndroidInitializationSettings('@mipmap/ic_launcher'),
        ),
      );
      _ready = true;
    } on Exception {
      _ready = false;
    }
  }

  Future<void> requestPermission() async {
    if (!_ready) {
      return;
    }
    final android = _plugin.resolvePlatformSpecificImplementation<
        AndroidFlutterLocalNotificationsPlugin>();
    await android?.requestNotificationsPermission();
  }

  Future<void> schedule(List<Lesson> lessons) async {
    if (!_ready) {
      return;
    }
    await _plugin.cancelAll();
    final now = tz.TZDateTime.now(tz.local);
    var id = 1;
    for (final lesson in lessons) {
      final time = _lessonTime(lesson);
      if (time == null) {
        continue;
      }
      final notificationTime = time.subtract(const Duration(minutes: 15));
      if (!notificationTime.isAfter(now)) {
        continue;
      }
      final body = lesson.room.isEmpty
          ? 'Через 15 минут: ${lesson.subject}'
          : 'Через 15 минут: ${lesson.subject}, ауд. ${lesson.room}';
      await _plugin.zonedSchedule(
        id: id,
        title: 'Скоро занятие',
        body: body,
        scheduledDate: notificationTime,
        notificationDetails: const NotificationDetails(
          android: AndroidNotificationDetails(
            'journey_lessons',
            'Занятия',
            channelDescription: 'Уведомления о начале занятий',
            importance: Importance.max,
            priority: Priority.high,
          ),
        ),
        androidScheduleMode: AndroidScheduleMode.inexactAllowWhileIdle,
        payload: '${lesson.date} ${lesson.startTime}',
      );
      id++;
    }
  }

  tz.TZDateTime? _lessonTime(Lesson lesson) {
    final parts = lesson.startTime.split(':');
    if (parts.length < 2) {
      return null;
    }
    final hour = int.tryParse(parts[0]);
    final minute = int.tryParse(parts[1]);
    if (hour == null || minute == null) {
      return null;
    }
    return tz.TZDateTime(
      tz.local,
      lesson.date.year,
      lesson.date.month,
      lesson.date.day,
      hour,
      minute,
    );
  }
}

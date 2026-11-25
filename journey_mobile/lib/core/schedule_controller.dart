import 'dart:async';

import 'package:connectivity_plus/connectivity_plus.dart';
import 'package:flutter/foundation.dart';
import 'package:intl/intl.dart';

import '../models/lesson.dart';
import 'api_client.dart';
import 'dates.dart';
import 'notification_service.dart';
import 'preferences.dart';
import 'schedule_database.dart';

enum ScheduleMode { day, week, month }

enum SyncState { idle, syncing, synced, offline, error }

class ScheduleController extends ChangeNotifier {
  ScheduleController({
    required ApiClient api,
    required AppPreferences preferences,
    required NotificationService notifications,
  })  : _api = api,
        _preferences = preferences,
        _notifications = notifications;

  final ApiClient _api;
  final AppPreferences _preferences;
  final NotificationService _notifications;
  final ScheduleDatabase _database = ScheduleDatabase();

  ScheduleMode _mode = ScheduleMode.day;
  DateTime _anchor = dateOnly(DateTime.now());
  List<Lesson> _lessons = [];
  SyncState _syncState = SyncState.idle;
  DateTime? _lastSync;
  DateTime? _lastAttempt;
  String? _syncError;
  bool _online = false;
  bool _loading = false;
  bool _started = false;
  bool _disposed = false;
  int _loadRequest = 0;
  Future<void>? _syncTask;
  StreamSubscription<List<ConnectivityResult>>? _connectionSubscription;

  ScheduleMode get mode => _mode;
  DateTime get anchor => _anchor;
  List<Lesson> get lessons => List.unmodifiable(_lessons);
  SyncState get syncState => _syncState;
  DateTime? get lastSync => _lastSync;
  DateTime? get lastAttempt => _lastAttempt;
  String? get syncError => _syncError;
  bool get online => _online;
  bool get loading => _loading;
  ScheduleDatabase get database => _database;

  DateTime get rangeStart {
    switch (_mode) {
      case ScheduleMode.day:
        return _anchor;
      case ScheduleMode.week:
        return startOfWeek(_anchor);
      case ScheduleMode.month:
        return startOfMonth(_anchor);
    }
  }

  DateTime get rangeEnd {
    switch (_mode) {
      case ScheduleMode.day:
        return _anchor;
      case ScheduleMode.week:
        return endOfWeek(_anchor);
      case ScheduleMode.month:
        return endOfMonth(_anchor);
    }
  }

  String get periodLabel {
    switch (_mode) {
      case ScheduleMode.day:
        return capitalize(DateFormat('EEEE, d MMMM', 'ru').format(_anchor));
      case ScheduleMode.week:
        final start = rangeStart;
        final end = rangeEnd;
        if (start.month == end.month) {
          return capitalize(
            '${start.day}–${end.day} ${DateFormat('MMMM', 'ru').format(end)}',
          );
        }
        return capitalize(
          '${start.day} ${DateFormat('MMM', 'ru').format(start)} – '
          '${end.day} ${DateFormat('MMM', 'ru').format(end)}',
        );
      case ScheduleMode.month:
        return capitalize(DateFormat('LLLL yyyy', 'ru').format(_anchor));
    }
  }

  Future<void> start() async {
    if (_started) {
      return;
    }
    _started = true;
    _lastSync = _preferences.lastSync;
    await _reload();
    await _scheduleNotifications();
    final connectivity = Connectivity();
    _connectionSubscription =
        connectivity.onConnectivityChanged.listen(_onConnectivityChanged);
    final initial = await connectivity.checkConnectivity();
    await _onConnectivityChanged(initial);
  }

  Future<void> setMode(ScheduleMode mode) async {
    if (_mode == mode) {
      return;
    }
    _mode = mode;
    _notify();
    await _reload();
  }

  Future<void> previous() async {
    switch (_mode) {
      case ScheduleMode.day:
        _anchor = _anchor.subtract(const Duration(days: 1));
        break;
      case ScheduleMode.week:
        _anchor = _anchor.subtract(const Duration(days: 7));
        break;
      case ScheduleMode.month:
        _anchor = DateTime(_anchor.year, _anchor.month - 1, 1);
        break;
    }
    await _reload();
  }

  Future<void> next() async {
    switch (_mode) {
      case ScheduleMode.day:
        _anchor = _anchor.add(const Duration(days: 1));
        break;
      case ScheduleMode.week:
        _anchor = _anchor.add(const Duration(days: 7));
        break;
      case ScheduleMode.month:
        _anchor = DateTime(_anchor.year, _anchor.month + 1, 1);
        break;
    }
    await _reload();
  }

  Future<void> goToToday() async {
    _anchor = dateOnly(DateTime.now());
    await _reload();
  }

  Future<void> sync() {
    final active = _syncTask;
    if (active != null) {
      return active;
    }
    final task = _runSync();
    _syncTask = task;
    task.whenComplete(() {
      _syncTask = null;
    });
    return task;
  }

  Future<void> _runSync() async {
    _syncState = SyncState.syncing;
    _syncError = null;
    _lastAttempt = DateTime.now();
    _notify();
    try {
      await _syncMonths(rangeStart, rangeEnd);
      final now = DateTime.now();
      _lastSync = now;
      await _preferences.saveLastSync(now);
      await _reload();
      _syncState = SyncState.synced;
      try {
        await _scheduleNotifications();
      } on Exception {
        _notify();
      }
    } on ApiException catch (error) {
      _syncState = _online ? SyncState.error : SyncState.offline;
      _syncError = error.message;
    } on Exception {
      _syncState = _online ? SyncState.error : SyncState.offline;
      _syncError = 'Не удалось обновить расписание';
    } finally {
      _notify();
    }
  }

  Future<void> _syncMonths(DateTime start, DateTime end) async {
    var month = DateTime(start.year, start.month, 1);
    final lastMonth = DateTime(end.year, end.month, 1);
    while (!month.isAfter(lastMonth)) {
      final data = await _api.get(
        '/schedule/operations/get-month',
        query: {'date_filter': dateKey(month)},
      );
      final fetched = _parseLessons(data);
      final monthLessons = fetched
          .where((lesson) =>
              lesson.date.year == month.year && lesson.date.month == month.month)
          .toList();
      await _database.replaceMonth(month, monthLessons);
      month = DateTime(month.year, month.month + 1, 1);
    }
  }

  List<Lesson> _parseLessons(dynamic data) {
    dynamic source = data;
    if (data is Map && data['lessons'] is List) {
      source = data['lessons'];
    }
    if (source == null) {
      return [];
    }
    if (source is! List) {
      throw const ApiException('Некорректный ответ сервера');
    }
    final result = <Lesson>[];
    for (final item in source) {
      if (item is Map) {
        final lesson = Lesson.fromApi(Map<String, dynamic>.from(item));
        if (lesson != null) {
          result.add(lesson);
        }
      }
    }
    return result;
  }

  Future<void> _onConnectivityChanged(List<ConnectivityResult> results) async {
    final connected =
        results.any((result) => result != ConnectivityResult.none);
    final regained = connected && !_online;
    _online = connected;
    if (!connected) {
      _syncState = SyncState.offline;
      _syncError = null;
      _notify();
      return;
    }
    if (regained) {
      await sync();
    } else {
      _notify();
    }
  }

  Future<void> _reload() async {
    final request = ++_loadRequest;
    final start = rangeStart;
    final end = rangeEnd;
    _loading = true;
    _notify();
    try {
      final lessons = await _database.lessonsBetween(start, end);
      if (request != _loadRequest || _disposed) {
        return;
      }
      _lessons = lessons;
    } on Exception {
      if (request != _loadRequest || _disposed) {
        return;
      }
      _lessons = [];
    } finally {
      if (request == _loadRequest && !_disposed) {
        _loading = false;
        _notify();
      }
    }
  }

  Future<void> _scheduleNotifications() async {
    final now = dateOnly(DateTime.now());
    final lessons = await _database.lessonsForNotification(
      now,
      now.add(const Duration(days: 31)),
    );
    await _notifications.schedule(lessons);
  }

  void _notify() {
    if (!_disposed) {
      notifyListeners();
    }
  }

  @override
  void dispose() {
    _disposed = true;
    unawaited(_connectionSubscription?.cancel());
    unawaited(_database.close());
    super.dispose();
  }
}

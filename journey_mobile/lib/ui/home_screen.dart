import 'package:material_ui/material_ui.dart';
import 'package:intl/intl.dart';

import '../core/api_client.dart';
import '../core/notification_service.dart';
import '../core/preferences.dart';
import '../core/schedule_controller.dart';
import 'lesson_list_view.dart';
import 'search_screen.dart';

class HomeScreen extends StatefulWidget {
  const HomeScreen({
    super.key,
    required this.api,
    required this.preferences,
    required this.theme,
  });

  final ApiClient api;
  final AppPreferences preferences;
  final ThemeController theme;

  @override
  State<HomeScreen> createState() => _HomeScreenState();
}

class _HomeScreenState extends State<HomeScreen> {
  late final ScheduleController _controller;

  @override
  void initState() {
    super.initState();
    _controller = ScheduleController(
      api: widget.api,
      preferences: widget.preferences,
      notifications: NotificationService.instance,
    );
    WidgetsBinding.instance.addPostFrameCallback((_) async {
      if (!mounted) {
        return;
      }
      await NotificationService.instance.requestPermission();
      if (!mounted) {
        return;
      }
      await _controller.start();
    });
  }

  @override
  void dispose() {
    _controller.dispose();
    super.dispose();
  }

  @override
  Widget build(BuildContext context) {
    return ListenableBuilder(
      listenable: _controller,
      builder: (context, child) {
        return Scaffold(
          appBar: AppBar(
            title: const Text('Journey'),
            actions: [
              IconButton(
                tooltip: 'Поиск',
                onPressed: () {
                  Navigator.of(context).push(
                    MaterialPageRoute(
                      builder: (_) => SearchScreen(
                        database: _controller.database,
                      ),
                    ),
                  );
                },
                icon: const Icon(Icons.search),
              ),
              IconButton(
                tooltip: 'Сменить тему',
                onPressed: widget.theme.toggle,
                icon: Icon(
                  widget.theme.isDark ? Icons.light_mode : Icons.dark_mode,
                ),
              ),
              if (_controller.syncState == SyncState.syncing)
                const Padding(
                  padding: EdgeInsets.symmetric(horizontal: 14),
                  child: Center(
                    child: SizedBox(
                      width: 20,
                      height: 20,
                      child: CircularProgressIndicator(strokeWidth: 2),
                    ),
                  ),
                )
              else
                IconButton(
                  tooltip: 'Обновить',
                  onPressed: _controller.sync,
                  icon: const Icon(Icons.refresh),
                ),
            ],
          ),
          body: Column(
            children: [
              _SyncStatusBar(controller: _controller),
              Padding(
                padding: const EdgeInsets.fromLTRB(16, 10, 16, 4),
                child: SegmentedButton<ScheduleMode>(
                  showSelectedIcon: false,
                  segments: const [
                    ButtonSegment(
                      value: ScheduleMode.day,
                      label: Text('День'),
                    ),
                    ButtonSegment(
                      value: ScheduleMode.week,
                      label: Text('Неделя'),
                    ),
                    ButtonSegment(
                      value: ScheduleMode.month,
                      label: Text('Месяц'),
                    ),
                  ],
                  selected: {_controller.mode},
                  onSelectionChanged: (selection) {
                    _controller.setMode(selection.first);
                  },
                ),
              ),
              Padding(
                padding: const EdgeInsets.fromLTRB(4, 0, 4, 4),
                child: Row(
                  children: [
                    IconButton(
                      tooltip: 'Назад',
                      onPressed: _controller.previous,
                      icon: const Icon(Icons.chevron_left),
                    ),
                    Expanded(
                      child: Text(
                        _controller.periodLabel,
                        textAlign: TextAlign.center,
                        maxLines: 2,
                        overflow: TextOverflow.ellipsis,
                        style: Theme.of(context)
                            .textTheme
                            .titleMedium
                            ?.copyWith(fontWeight: FontWeight.w600),
                      ),
                    ),
                    IconButton(
                      tooltip: 'Вперёд',
                      onPressed: _controller.next,
                      icon: const Icon(Icons.chevron_right),
                    ),
                    IconButton(
                      tooltip: 'Сегодня',
                      onPressed: _controller.goToToday,
                      icon: const Icon(Icons.today_outlined),
                    ),
                  ],
                ),
              ),
              Expanded(
                child: _controller.loading && _controller.lessons.isEmpty
                    ? const Center(child: CircularProgressIndicator())
                    : LessonListView(
                        lessons: _controller.lessons,
                        showDateHeaders: _controller.mode != ScheduleMode.day,
                      ),
              ),
            ],
          ),
        );
      },
    );
  }
}

class _SyncStatusBar extends StatelessWidget {
  const _SyncStatusBar({required this.controller});

  final ScheduleController controller;

  @override
  Widget build(BuildContext context) {
    final colors = Theme.of(context).colorScheme;
    final time = controller.lastSync ?? controller.lastAttempt;
    final formatted = time == null ? '' : DateFormat('HH:mm').format(time);
    IconData icon = Icons.cloud_queue;
    Color color = colors.onSurfaceVariant;
    String text = 'Нет данных о синхронизации';
    Widget? leading;

    switch (controller.syncState) {
      case SyncState.syncing:
        icon = Icons.sync;
        color = colors.primary;
        text = 'Синхронизация…';
        leading = SizedBox(
          width: 16,
          height: 16,
          child: CircularProgressIndicator(strokeWidth: 2, color: color),
        );
      case SyncState.synced:
        icon = Icons.cloud_done_outlined;
        color = colors.primary;
        text = formatted.isEmpty
            ? 'Синхронизировано'
            : 'Синхронизировано в $formatted';
      case SyncState.offline:
        icon = Icons.cloud_off_outlined;
        text = formatted.isEmpty
            ? 'Офлайн-режим'
            : 'Офлайн-режим · данные от $formatted';
      case SyncState.error:
        icon = Icons.error_outline;
        color = colors.error;
        text = controller.syncError ?? 'Не удалось обновить расписание';
      case SyncState.idle:
        break;
    }

    return Container(
      width: double.infinity,
      color: colors.surfaceContainerHighest,
      padding: const EdgeInsets.symmetric(horizontal: 16, vertical: 8),
      child: Row(
        children: [
          leading ?? Icon(icon, size: 18, color: color),
          const SizedBox(width: 8),
          Expanded(
            child: Text(
              text,
              maxLines: 2,
              overflow: TextOverflow.ellipsis,
              style: Theme.of(context)
                  .textTheme
                  .bodySmall
                  ?.copyWith(color: color),
            ),
          ),
        ],
      ),
    );
  }
}

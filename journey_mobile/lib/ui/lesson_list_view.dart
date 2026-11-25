import 'package:material_ui/material_ui.dart';
import 'package:intl/intl.dart';

import '../core/dates.dart';
import '../models/lesson.dart';
import 'lesson_card.dart';

class LessonListView extends StatelessWidget {
  const LessonListView({
    super.key,
    required this.lessons,
    required this.showDateHeaders,
    this.emptyTitle = 'Занятий нет',
    this.emptyText = 'На этот период занятия не запланированы',
  });

  final List<Lesson> lessons;
  final bool showDateHeaders;
  final String emptyTitle;
  final String emptyText;

  @override
  Widget build(BuildContext context) {
    if (lessons.isEmpty) {
      final colors = Theme.of(context).colorScheme;
      return Center(
        child: Padding(
          padding: const EdgeInsets.all(32),
          child: Column(
            mainAxisSize: MainAxisSize.min,
            children: [
              Icon(Icons.event_busy, size: 48, color: colors.onSurfaceVariant),
              const SizedBox(height: 12),
              Text(
                emptyTitle,
                style: Theme.of(context).textTheme.titleMedium,
                textAlign: TextAlign.center,
              ),
              const SizedBox(height: 4),
              Text(
                emptyText,
                style: Theme.of(context).textTheme.bodySmall?.copyWith(
                      color: colors.onSurfaceVariant,
                    ),
                textAlign: TextAlign.center,
              ),
            ],
          ),
        ),
      );
    }

    final grouped = <DateTime, List<Lesson>>{};
    for (final lesson in lessons) {
      grouped.putIfAbsent(lesson.date, () => []).add(lesson);
    }
    final dates = grouped.keys.toList()..sort();
    final children = <Widget>[];
    for (final date in dates) {
      if (showDateHeaders) {
        children.add(
          Padding(
            padding: const EdgeInsets.only(top: 12, bottom: 8),
            child: Text(
              capitalize(DateFormat('EEEE, d MMMM', 'ru').format(date)),
              style: Theme.of(context).textTheme.titleSmall?.copyWith(
                    fontWeight: FontWeight.w700,
                  ),
            ),
          ),
        );
      }
      for (final lesson in grouped[date]!) {
        children.add(LessonCard(lesson: lesson));
      }
    }
    return ListView(
      padding: const EdgeInsets.fromLTRB(16, 4, 16, 24),
      children: children,
    );
  }
}

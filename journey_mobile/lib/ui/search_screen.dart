import 'dart:async';

import 'package:material_ui/material_ui.dart';

import '../core/schedule_database.dart';
import '../models/lesson.dart';
import 'lesson_list_view.dart';

class SearchScreen extends StatefulWidget {
  const SearchScreen({super.key, required this.database});

  final ScheduleDatabase database;

  @override
  State<SearchScreen> createState() => _SearchScreenState();
}

class _SearchScreenState extends State<SearchScreen> {
  final TextEditingController _controller = TextEditingController();
  Timer? _debounce;
  List<Lesson> _results = [];
  bool _searching = false;
  bool _emptyQuery = true;
  int _request = 0;

  @override
  void dispose() {
    _debounce?.cancel();
    _controller.dispose();
    super.dispose();
  }

  void _onChanged(String value) {
    _debounce?.cancel();
    final query = value.trim();
    if (query.isEmpty) {
      setState(() {
        _emptyQuery = true;
        _searching = false;
        _results = [];
      });
      return;
    }
    setState(() {
      _emptyQuery = false;
      _searching = true;
    });
    _debounce = Timer(const Duration(milliseconds: 300), () {
      _search(query);
    });
  }

  Future<void> _search(String query) async {
    final request = ++_request;
    final results = await widget.database.search(query);
    if (!mounted || request != _request) {
      return;
    }
    setState(() {
      _results = results;
      _searching = false;
    });
  }

  @override
  Widget build(BuildContext context) {
    return Scaffold(
      appBar: AppBar(title: const Text('Поиск')),
      body: Column(
        children: [
          Padding(
            padding: const EdgeInsets.fromLTRB(16, 8, 16, 8),
            child: TextField(
              controller: _controller,
              autofocus: true,
              textInputAction: TextInputAction.search,
              onChanged: _onChanged,
              decoration: InputDecoration(
                hintText: 'Предмет, преподаватель или аудитория',
                prefixIcon: const Icon(Icons.search),
                suffixIcon: _emptyQuery
                    ? null
                    : IconButton(
                        tooltip: 'Очистить',
                        onPressed: () {
                          _controller.clear();
                          _onChanged('');
                        },
                        icon: const Icon(Icons.close),
                      ),
              ),
            ),
          ),
          Expanded(
            child: _emptyQuery
                ? const _SearchHint()
                : _searching
                    ? const Center(child: CircularProgressIndicator())
                    : LessonListView(
                        lessons: _results,
                        showDateHeaders: true,
                        emptyTitle: 'Ничего не найдено',
                        emptyText: 'Попробуйте изменить запрос',
                      ),
          ),
        ],
      ),
    );
  }
}

class _SearchHint extends StatelessWidget {
  const _SearchHint();

  @override
  Widget build(BuildContext context) {
    final colors = Theme.of(context).colorScheme;
    return Center(
      child: Padding(
        padding: const EdgeInsets.all(32),
        child: Column(
          mainAxisSize: MainAxisSize.min,
          children: [
            Icon(Icons.search, size: 48, color: colors.onSurfaceVariant),
            const SizedBox(height: 12),
            Text(
              'Поиск по расписанию',
              style: Theme.of(context).textTheme.titleMedium,
            ),
            const SizedBox(height: 4),
            Text(
              'Введите название предмета, преподавателя или аудиторию',
              textAlign: TextAlign.center,
              style: Theme.of(context).textTheme.bodySmall?.copyWith(
                    color: colors.onSurfaceVariant,
                  ),
            ),
          ],
        ),
      ),
    );
  }
}

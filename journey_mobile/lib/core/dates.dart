DateTime dateOnly(DateTime value) => DateTime(value.year, value.month, value.day);

String dateKey(DateTime value) =>
    '${value.year.toString().padLeft(4, '0')}-${value.month.toString().padLeft(2, '0')}-${value.day.toString().padLeft(2, '0')}';

DateTime? parseDate(String value) {
  final trimmed = value.trim();
  if (trimmed.isEmpty) {
    return null;
  }
  final parsed = DateTime.tryParse(trimmed);
  return parsed == null ? null : dateOnly(parsed);
}

String normalizeTime(String value) {
  final parts = value.trim().split(':');
  if (parts.length < 2) {
    return value.trim();
  }
  return '${parts[0].padLeft(2, '0')}:${parts[1].padLeft(2, '0')}';
}

DateTime startOfWeek(DateTime value) {
  final day = dateOnly(value);
  return day.subtract(Duration(days: day.weekday - 1));
}

DateTime endOfWeek(DateTime value) => startOfWeek(value).add(const Duration(days: 6));

DateTime startOfMonth(DateTime value) => DateTime(value.year, value.month, 1);

DateTime endOfMonth(DateTime value) => DateTime(value.year, value.month + 1, 0);

String capitalize(String value) {
  if (value.isEmpty) {
    return value;
  }
  return value[0].toUpperCase() + value.substring(1);
}

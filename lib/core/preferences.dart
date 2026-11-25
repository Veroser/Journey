import 'package:material_ui/material_ui.dart';
import 'package:shared_preferences/shared_preferences.dart';

class AppPreferences {
  AppPreferences(this._prefs);

  static const String _darkThemeKey = 'dark_theme';
  static const String _lastSyncKey = 'last_sync';

  final SharedPreferences _prefs;

  bool get darkTheme => _prefs.getBool(_darkThemeKey) ?? false;

  Future<void> saveDarkTheme(bool value) => _prefs.setBool(_darkThemeKey, value);

  DateTime? get lastSync {
    final value = _prefs.getInt(_lastSyncKey);
    return value == null ? null : DateTime.fromMillisecondsSinceEpoch(value);
  }

  Future<void> saveLastSync(DateTime value) =>
      _prefs.setInt(_lastSyncKey, value.millisecondsSinceEpoch);
}

class ThemeController extends ChangeNotifier {
  ThemeController(this._prefs) : _dark = _prefs.darkTheme;

  final AppPreferences _prefs;
  bool _dark;

  bool get isDark => _dark;
  ThemeMode get mode => _dark ? ThemeMode.dark : ThemeMode.light;

  Future<void> toggle() async {
    _dark = !_dark;
    await _prefs.saveDarkTheme(_dark);
    notifyListeners();
  }
}

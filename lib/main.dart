import 'package:material_ui/material_ui.dart';
import 'package:intl/date_symbol_data_local.dart';
import 'package:shared_preferences/shared_preferences.dart';

import 'app.dart';
import 'core/notification_service.dart';
import 'core/preferences.dart';

Future<void> main() async {
  WidgetsFlutterBinding.ensureInitialized();
  await initializeDateFormatting('ru_RU');
  await NotificationService.instance.initialize();
  final prefs = await SharedPreferences.getInstance();
  final preferences = AppPreferences(prefs);
  runApp(
    JourneyApp(
      preferences: preferences,
      theme: ThemeController(preferences),
    ),
  );
}

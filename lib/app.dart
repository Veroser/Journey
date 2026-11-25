import 'package:cupertino_ui/cupertino_ui.dart' show GlobalCupertinoLocalizations;
import 'package:flutter_localizations/flutter_localizations.dart'
    show GlobalWidgetsLocalizations;
import 'package:flutter_secure_storage/flutter_secure_storage.dart';
import 'package:material_ui/material_ui.dart';

import 'core/api_client.dart';
import 'core/auth_controller.dart';
import 'core/preferences.dart';
import 'ui/home_screen.dart';
import 'ui/login_screen.dart';

class JourneyApp extends StatefulWidget {
  const JourneyApp({
    super.key,
    required this.preferences,
    required this.theme,
  });

  final AppPreferences preferences;
  final ThemeController theme;

  @override
  State<JourneyApp> createState() => _JourneyAppState();
}

class _JourneyAppState extends State<JourneyApp> {
  late final ApiClient _api;
  late final AuthController _auth;

  @override
  void initState() {
    super.initState();
    _api = ApiClient();
    _auth = AuthController(
      api: _api,
      storage: const FlutterSecureStorage(),
    );
    _auth.initialize();
  }

  @override
  void dispose() {
    _auth.dispose();
    super.dispose();
  }

  @override
  Widget build(BuildContext context) {
    return ListenableBuilder(
      listenable: Listenable.merge([_auth, widget.theme]),
      builder: (context, child) {
        return MaterialApp(
          title: 'Journey',
          debugShowCheckedModeBanner: false,
          theme: _theme(Brightness.light),
          darkTheme: _theme(Brightness.dark),
          themeMode: widget.theme.mode,
          locale: const Locale('ru', 'RU'),
          supportedLocales: const [Locale('ru', 'RU')],
          localizationsDelegates: const [
            GlobalMaterialLocalizations.delegate,
            GlobalWidgetsLocalizations.delegate,
            GlobalCupertinoLocalizations.delegate,
          ],
          home: switch (_auth.status) {
            AuthStatus.loading => const _SplashScreen(),
            AuthStatus.unauthenticated => LoginScreen(
                auth: _auth,
                theme: widget.theme,
              ),
            AuthStatus.authenticated => HomeScreen(
                api: _api,
                preferences: widget.preferences,
                theme: widget.theme,
              ),
          },
        );
      },
    );
  }

  ThemeData _theme(Brightness brightness) {
    final scheme = ColorScheme.fromSeed(
      seedColor: const Color(0xFF4CAF50),
      brightness: brightness,
    );
    return ThemeData(
      useMaterial3: true,
      colorScheme: scheme,
      scaffoldBackgroundColor: scheme.surface,
      appBarTheme: AppBarTheme(
        backgroundColor: scheme.surface,
        surfaceTintColor: Colors.transparent,
        elevation: 0,
        centerTitle: false,
      ),
      inputDecorationTheme: InputDecorationThemeData(
        filled: true,
        border: OutlineInputBorder(
          borderRadius: BorderRadius.circular(14),
          borderSide: BorderSide.none,
        ),
      ),
    );
  }
}

class _SplashScreen extends StatelessWidget {
  const _SplashScreen();

  @override
  Widget build(BuildContext context) {
    return const Scaffold(
      body: Center(child: CircularProgressIndicator()),
    );
  }
}

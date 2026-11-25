import 'dart:convert';

import 'package:flutter/foundation.dart';
import 'package:flutter_secure_storage/flutter_secure_storage.dart';

import 'api_client.dart';

enum AuthStatus { loading, unauthenticated, authenticated }

class AuthController extends ChangeNotifier {
  AuthController({
    required ApiClient api,
    required FlutterSecureStorage storage,
  })  : _api = api,
        _storage = storage {
    _api.onRefresh = _refresh;
  }

  static const String _accessKey = 'access_token';
  static const String _refreshKey = 'refresh_token';
  static const String _expiresKey = 'expires_in_access';
  static const String _rememberKey = 'remember_credentials';
  static const String _usernameKey = 'username';
  static const String _passwordKey = 'password';

  final ApiClient _api;
  final FlutterSecureStorage _storage;

  AuthStatus _status = AuthStatus.loading;
  bool _loggingIn = false;
  bool _hasSavedCredentials = false;
  String? _savedUsername;
  String? _savedPassword;
  Future<bool>? _refreshTask;

  AuthStatus get status => _status;
  bool get isAuthenticated => _status == AuthStatus.authenticated;
  bool get isLoggingIn => _loggingIn;
  bool get hasSavedCredentials => _hasSavedCredentials;
  String? get savedUsername => _savedUsername;
  String? get savedPassword => _savedPassword;

  Future<void> initialize() async {
    final access = await _read(_accessKey);
    final refresh = await _read(_refreshKey);
    final expiry = int.tryParse(await _read(_expiresKey) ?? '');
    await _loadSavedCredentials();

    if (access == null || access.isEmpty) {
      _status = AuthStatus.unauthenticated;
      notifyListeners();
      return;
    }

    _api.setToken(access);
    final expiresAt = expiry ?? _jwtExpiry(access) ?? 0;
    if (expiresAt > DateTime.now().millisecondsSinceEpoch ~/ 1000 + 60) {
      _status = AuthStatus.authenticated;
      notifyListeners();
      return;
    }

    if (refresh == null || refresh.isEmpty) {
      await _clearSession();
      _status = AuthStatus.unauthenticated;
      notifyListeners();
      return;
    }

    final refreshed = await _refresh();
    _status = refreshed ? AuthStatus.authenticated : AuthStatus.unauthenticated;
    notifyListeners();
  }

  Future<void> login({
    required String username,
    required String password,
    required bool remember,
  }) async {
    _loggingIn = true;
    notifyListeners();
    try {
      final data = await _api.post(
        '/auth/login',
        {
          'application_key': ApiClient.applicationKey,
          'id_city': null,
          'username': username,
          'password': password,
        },
        auth: false,
      );
      if (data is! Map) {
        throw const ApiException('Некорректный ответ сервера');
      }
      final json = Map<String, dynamic>.from(data);
      final access = json['access_token']?.toString() ?? '';
      if (access.isEmpty) {
        throw const ApiException('Сервер не вернул токен доступа');
      }
      final refresh = json['refresh_token']?.toString() ?? await _read(_refreshKey) ?? '';
      final expiresAt = _asInt(json['expires_in_access']) ?? _jwtExpiry(access) ?? 0;

      _api.setToken(access);
      await _saveSession(access, refresh, expiresAt);

      if (remember) {
        await _storage.write(key: _rememberKey, value: 'true');
        await _storage.write(key: _usernameKey, value: username);
        await _storage.write(key: _passwordKey, value: password);
        _hasSavedCredentials = true;
        _savedUsername = username;
        _savedPassword = password;
      } else {
        await _storage.delete(key: _rememberKey);
        await _storage.delete(key: _usernameKey);
        await _storage.delete(key: _passwordKey);
        _hasSavedCredentials = false;
        _savedUsername = null;
        _savedPassword = null;
      }

      _status = AuthStatus.authenticated;
    } finally {
      _loggingIn = false;
      notifyListeners();
    }
  }

  Future<bool> _refresh() {
    final active = _refreshTask;
    if (active != null) {
      return active;
    }
    final task = _performRefresh();
    _refreshTask = task;
    task.whenComplete(() {
      _refreshTask = null;
    });
    return task;
  }

  Future<bool> _performRefresh() async {
    final refresh = await _read(_refreshKey);
    if (refresh == null || refresh.isEmpty) {
      await _clearSession();
      _status = AuthStatus.unauthenticated;
      notifyListeners();
      return false;
    }
    try {
      final data = await _api.post(
        '/auth/refresh',
        {
          'application_key': ApiClient.applicationKey,
          'refresh_token': refresh,
        },
        auth: false,
      );
      if (data is! Map) {
        throw const ApiException('Некорректный ответ сервера');
      }
      final json = Map<String, dynamic>.from(data);
      final access = json['access_token']?.toString() ?? '';
      if (access.isEmpty) {
        throw const ApiException('Сервер не вернул токен доступа');
      }
      final nextRefresh = json['refresh_token']?.toString() ?? refresh;
      final expiresAt = _asInt(json['expires_in_access']) ?? _jwtExpiry(access) ?? 0;
      _api.setToken(access);
      await _saveSession(access, nextRefresh, expiresAt);
      _status = AuthStatus.authenticated;
      notifyListeners();
      return true;
    } on Exception {
      await _clearSession();
      _status = AuthStatus.unauthenticated;
      notifyListeners();
      return false;
    }
  }

  Future<void> _saveSession(String access, String refresh, int expiresAt) async {
    await _storage.write(key: _accessKey, value: access);
    await _storage.write(key: _refreshKey, value: refresh);
    await _storage.write(key: _expiresKey, value: expiresAt.toString());
  }

  Future<void> _clearSession() async {
    await _storage.delete(key: _accessKey);
    await _storage.delete(key: _refreshKey);
    await _storage.delete(key: _expiresKey);
    _api.setToken(null);
  }

  Future<void> _loadSavedCredentials() async {
    final remember = await _read(_rememberKey);
    if (remember != 'true') {
      _hasSavedCredentials = false;
      _savedUsername = null;
      _savedPassword = null;
      return;
    }
    _savedUsername = await _read(_usernameKey);
    _savedPassword = await _read(_passwordKey);
    _hasSavedCredentials =
        (_savedUsername != null && _savedUsername!.isNotEmpty) &&
            (_savedPassword != null && _savedPassword!.isNotEmpty);
  }

  Future<String?> _read(String key) async {
    try {
      return await _storage.read(key: key);
    } on Exception {
      return null;
    }
  }

  int? _jwtExpiry(String token) {
    try {
      final parts = token.split('.');
      if (parts.length < 2) {
        return null;
      }
      final payload = jsonDecode(
        utf8.decode(base64Url.decode(base64Url.normalize(parts[1]))),
      );
      if (payload is Map) {
        return _asInt(payload['exp']);
      }
    } on Exception {
      return null;
    }
    return null;
  }

  int? _asInt(Object? value) {
    if (value is int) {
      return value;
    }
    return int.tryParse(value?.toString() ?? '');
  }
}

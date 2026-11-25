import 'dart:async';
import 'dart:convert';

import 'package:flutter/foundation.dart' show kIsWeb;
import 'package:http/http.dart' as http;

import 'connection_error.dart';

class ApiException implements Exception {
  const ApiException(this.message, {this.statusCode});

  final String message;
  final int? statusCode;

  @override
  String toString() => message;
}

class ApiClient {
  ApiClient({http.Client? client}) : _client = client ?? http.Client();

  static const String _remoteBaseUrl = 'https://msapi.top-academy.ru/api/v2';

  /// На web браузер не может ходить напрямую в API из-за CORS и DDoS-Guard,
  /// поэтому по умолчанию используется локальный прокси (tool/api_proxy.dart).
  /// Переопределить можно через --dart-define=API_BASE_URL=...
  static const String _webBaseUrl = String.fromEnvironment(
    'API_BASE_URL',
    defaultValue: 'http://localhost:8787/api/v2',
  );

  static const String _nativeBaseUrl = String.fromEnvironment(
    'API_BASE_URL',
    defaultValue: _remoteBaseUrl,
  );

  static String get baseUrl => kIsWeb ? _webBaseUrl : _nativeBaseUrl;

  static const String applicationKey =
      '6a56a5df2667e65aab73ce76d1dd737f7d1faef9c52e8b8c55ac75f565d8e8a6';

  final http.Client _client;
  String? _token;
  Future<bool> Function()? onRefresh;

  void setToken(String? token) {
    _token = token;
  }

  Future<dynamic> get(
    String path, {
    Map<String, String>? query,
    bool auth = true,
  }) {
    return _request('GET', path, query: query, auth: auth);
  }

  Future<dynamic> post(
    String path,
    Map<String, dynamic> body, {
    bool auth = true,
  }) {
    return _request('POST', path, body: body, auth: auth);
  }

  Future<dynamic> _request(
    String method,
    String path, {
    Map<String, String>? query,
    Map<String, dynamic>? body,
    bool auth = true,
  }) async {
    var response = await _send(method, path, query: query, body: body, auth: auth);
    if (response.statusCode == 401 && auth && onRefresh != null) {
      final refreshed = await onRefresh!();
      if (refreshed) {
        response = await _send(method, path, query: query, body: body, auth: auth);
      }
    }
    if (response.statusCode < 200 || response.statusCode >= 300) {
      throw ApiException(
        _errorMessage(response, auth: auth),
        statusCode: response.statusCode,
      );
    }
    final text = utf8.decode(response.bodyBytes);
    if (text.trim().isEmpty) {
      return null;
    }
    try {
      return jsonDecode(text);
    } on FormatException {
      throw const ApiException('Ошибка обработки ответа сервера');
    }
  }

  Future<http.Response> _send(
    String method,
    String path, {
    Map<String, String>? query,
    Map<String, dynamic>? body,
    required bool auth,
  }) async {
    final uri = Uri.parse('$baseUrl$path').replace(queryParameters: query);
    try {
      if (method == 'POST') {
        return await _client
            .post(uri, headers: _headers(auth: auth), body: jsonEncode(body ?? {}))
            .timeout(const Duration(seconds: 30));
      }
      return await _client
          .get(uri, headers: _headers(auth: auth))
          .timeout(const Duration(seconds: 30));
    } on TimeoutException {
      throw const ApiException('Сервер не ответил вовремя');
    } on http.ClientException {
      throw ApiException(_connectionMessage);
    } on Exception catch (error) {
      if (isConnectionError(error)) {
        throw ApiException(_connectionMessage);
      }
      rethrow;
    }
  }

  String get _connectionMessage => kIsWeb
      ? 'Нет подключения к серверу. Проверьте, запущен ли прокси (dart run tool/api_proxy.dart)'
      : 'Нет подключения к интернету';

  Map<String, String> _headers({required bool auth}) {
    final headers = <String, String>{
      'Content-Type': 'application/json',
      'Accept': 'application/json, text/plain, */*',
      'Origin': 'https://journal.top-academy.ru',
      'Referer': 'https://journal.top-academy.ru/',
      'User-Agent': 'Journey/1.0 (Android)',
    };
    if (auth && _token != null && _token!.isNotEmpty) {
      headers['Authorization'] = 'Bearer $_token';
    }
    return headers;
  }

  String _errorMessage(http.Response response, {required bool auth}) {
    if (response.statusCode == 401) {
      return auth ? 'Сессия истекла, войдите заново' : 'Неверный логин или пароль';
    }
    if (response.statusCode == 403) {
      return 'Доступ запрещён';
    }
    if (response.statusCode == 404) {
      return 'Данные не найдены';
    }
    if (response.statusCode >= 500) {
      return 'Ошибка сервера';
    }
    try {
      final decoded = jsonDecode(utf8.decode(response.bodyBytes));
      final message = _extractMessage(decoded);
      if (message != null) {
        return message;
      }
    } on FormatException {
      return 'Ошибка запроса';
    }
    return 'Ошибка запроса';
  }

  String? _extractMessage(Object? decoded) {
    if (decoded is Map) {
      final message = decoded['message'] ?? decoded['error'];
      if (message is String && message.trim().isNotEmpty) {
        return message.trim();
      }
      return null;
    }
    if (decoded is List) {
      for (final item in decoded) {
        final message = _extractMessage(item);
        if (message != null) {
          return message;
        }
      }
    }
    return null;
  }
}

// Локальный прокси для web-версии Journey.
//
// Зачем он нужен:
//  * API (msapi.top-academy.ru) закрыт DDoS-Guard и не отдаёт CORS-заголовки,
//    поэтому браузер не может обращаться к нему напрямую;
//  * прокси принимает запросы от браузера на http://localhost:8787,
//    сам ходит в API с нужными заголовками (Origin/Referer journal.top-academy.ru)
//    и добавляет CORS-заголовки в ответ;
//  * этот же сервер раздаёт собранное приложение из build/web,
//    так что всё работает на одном origin.
//
// Запуск:  dart run tool/api_proxy.dart
// Открыть: http://localhost:8787

import 'dart:async';
import 'dart:convert';
import 'dart:io';

const int _defaultPort = 8787;
const String _upstream = 'https://msapi.top-academy.ru';
const String _upstreamOrigin = 'https://journal.top-academy.ru';
const String _userAgent =
    'Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 '
    '(KHTML, like Gecko) Chrome/131.0.0.0 Safari/537.36';

const Set<String> _skipRequestHeaders = {
  'host',
  'origin',
  'referer',
  'cookie',
  'content-length',
  'connection',
  'keep-alive',
  'proxy-connection',
  'transfer-encoding',
  'upgrade',
  'accept-encoding',
  'sec-fetch-dest',
  'sec-fetch-mode',
  'sec-fetch-site',
  'sec-ch-ua',
  'sec-ch-ua-mobile',
  'sec-ch-ua-platform',
};

const Set<String> _skipResponseHeaders = {
  'connection',
  'keep-alive',
  'transfer-encoding',
  'content-length',
  'set-cookie',
  'set-cookie2',
  'content-security-policy',
  'strict-transport-security',
};

const Map<String, String> _corsHeaders = {
  'Access-Control-Allow-Origin': '*',
  'Access-Control-Allow-Methods': 'GET, POST, PUT, PATCH, DELETE, OPTIONS',
  'Access-Control-Allow-Headers': 'Content-Type, Authorization, Accept',
  'Access-Control-Max-Age': '86400',
};

const Map<String, String> _contentTypes = {
  '.html': 'text/html; charset=utf-8',
  '.js': 'text/javascript; charset=utf-8',
  '.mjs': 'text/javascript; charset=utf-8',
  '.json': 'application/json; charset=utf-8',
  '.css': 'text/css; charset=utf-8',
  '.wasm': 'application/wasm',
  '.png': 'image/png',
  '.jpg': 'image/jpeg',
  '.jpeg': 'image/jpeg',
  '.gif': 'image/gif',
  '.svg': 'image/svg+xml',
  '.ico': 'image/x-icon',
  '.ttf': 'font/ttf',
  '.otf': 'font/otf',
  '.woff': 'font/woff',
  '.woff2': 'font/woff2',
  '.map': 'application/json; charset=utf-8',
  '.txt': 'text/plain; charset=utf-8',
};

Future<void> main(List<String> args) async {
  final port = _readPort(args);
  final webDir = Directory(
    '${Directory.current.path}${Platform.pathSeparator}build'
    '${Platform.pathSeparator}web',
  );

  final client = HttpClient()..autoUncompress = false;
  final server = await HttpServer.bind(InternetAddress.loopbackIPv4, port);
  server.autoCompress = false;

  stdout.writeln('Journey web: http://localhost:$port');
  if (!webDir.existsSync()) {
    stdout.writeln(
      'Папка build/web не найдена. Соберите приложение: flutter build web',
    );
  }

  await for (final request in server) {
    unawaited(_handle(request, client, webDir));
  }
}

int _readPort(List<String> args) {
  final env = Platform.environment['PORT'];
  if (env != null) {
    final parsed = int.tryParse(env);
    if (parsed != null) {
      return parsed;
    }
  }
  for (var i = 0; i < args.length; i++) {
    if (args[i] == '--port' && i + 1 < args.length) {
      final parsed = int.tryParse(args[i + 1]);
      if (parsed != null) {
        return parsed;
      }
    }
  }
  return _defaultPort;
}

Future<void> _handle(
  HttpRequest request,
  HttpClient client,
  Directory webDir,
) async {
  try {
    if (request.method == 'OPTIONS') {
      final response = request.response..statusCode = HttpStatus.noContent;
      _corsHeaders.forEach(response.headers.set);
      response.headers.set(
        'Access-Control-Allow-Headers',
        request.headers.value('access-control-request-headers') ??
            'Content-Type, Authorization',
      );
      await response.close();
      return;
    }

    final path = request.uri.path;
    if (path == '/api' || path.startsWith('/api/')) {
      await _proxy(request, client);
      return;
    }
    await _serveStatic(request, webDir);
  } on Exception catch (error) {
    stderr.writeln('Ошибка обработки ${request.uri}: $error');
    try {
      final response = request.response;
      response.statusCode = HttpStatus.internalServerError;
      _corsHeaders.forEach(response.headers.set);
      response.headers.contentType = ContentType.json;
      response.write(jsonEncode({'message': 'Ошибка прокси: $error'}));
      await response.close();
    } on Exception {
      // Соединение уже закрыто — игнорируем.
    }
  }
}

Future<void> _proxy(HttpRequest request, HttpClient client) async {
  final upstreamUri = Uri.parse(
    '$_upstream${request.uri.path}'
    '${request.uri.hasQuery ? '?${request.uri.query}' : ''}',
  );

  final upstreamRequest = await client.openUrl(request.method, upstreamUri);
  request.headers.forEach((name, values) {
    if (_skipRequestHeaders.contains(name.toLowerCase())) {
      return;
    }
    for (final value in values) {
      upstreamRequest.headers.add(name, value);
    }
  });
  upstreamRequest.headers.set('Origin', _upstreamOrigin);
  upstreamRequest.headers.set('Referer', '$_upstreamOrigin/');
  upstreamRequest.headers.set('User-Agent', _userAgent);
  upstreamRequest.headers.set('Accept-Language', 'ru-RU,ru;q=0.9,en;q=0.8');

  final body = await request.fold<List<int>>(
    <int>[],
    (buffer, chunk) => buffer..addAll(chunk),
  );
  if (body.isNotEmpty) {
    upstreamRequest.add(body);
  }

  final upstreamResponse = await upstreamRequest.close();
  final response = request.response;
  response.statusCode = upstreamResponse.statusCode;
  upstreamResponse.headers.forEach((name, values) {
    if (_skipResponseHeaders.contains(name.toLowerCase())) {
      return;
    }
    for (final value in values) {
      response.headers.add(name, value);
    }
  });
  _corsHeaders.forEach(response.headers.set);
  response.headers.set('Cache-Control', 'no-store');
  await upstreamResponse.pipe(response);
}

Future<void> _serveStatic(HttpRequest request, Directory webDir) async {
  final response = request.response;
  if (!webDir.existsSync()) {
    response.statusCode = HttpStatus.notFound;
    response.headers.contentType = ContentType.html;
    response.write(
      '<h1>build/web не найден</h1>'
      '<p>Выполните <code>flutter build web</code> и перезапустите прокси.</p>',
    );
    await response.close();
    return;
  }

  var relative = Uri.decodeComponent(request.uri.path);
  if (relative == '/' || relative.isEmpty) {
    relative = '/index.html';
  }
  relative = relative.replaceAll('\\', '/');
  while (relative.startsWith('/')) {
    relative = relative.substring(1);
  }

  final root = webDir.absolute.path;
  var file = File('$root${Platform.pathSeparator}'
      '${relative.replaceAll('/', Platform.pathSeparator)}');

  if (!file.existsSync()) {
    // SPA-фолбэк: отдаём index.html для «красивых» маршрутов.
    final hasExtension = relative.split('/').last.contains('.');
    if (!hasExtension) {
      file = File('$root${Platform.pathSeparator}index.html');
    }
  }

  if (!file.existsSync()) {
    response.statusCode = HttpStatus.notFound;
    response.headers.contentType = ContentType.text;
    response.write('Not found: /$relative');
    await response.close();
    return;
  }

  final extension =
      file.path.substring(file.path.lastIndexOf('.')).toLowerCase();
  response.headers.contentType = ContentType.parse(
    _contentTypes[extension] ?? 'application/octet-stream',
  );
  if (extension == '.html' || extension == '.js' || extension == '.json') {
    response.headers.set('Cache-Control', 'no-cache');
  }
  await response.addStream(file.openRead());
  await response.close();
}

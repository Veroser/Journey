# Journey

Flutter-приложение с расписанием занятий (Journey).

## Запуск в браузере

Приложение работает в браузере через локальный прокси: API
`msapi.top-academy.ru` закрыт DDoS-Guard и не отдаёт CORS-заголовки, поэтому
браузер не может обращаться к нему напрямую.

Самый простой способ:

```powershell
.\run_web.ps1
```

Скрипт соберёт web-версию, запустит прокси на `http://localhost:8787` и откроет
браузер. Повторный запуск без пересборки: `.\run_web.ps1 -SkipBuild`.

### Вручную

```powershell
flutter build web
dart run tool/api_proxy.dart
# открыть http://localhost:8787
```

### Режим разработки (hot reload)

В одном терминале:

```powershell
dart run tool/api_proxy.dart
```

В другом:

```powershell
flutter run -d edge
```

Web-сборка по умолчанию обращается к `http://localhost:8787/api/v2`
(прокси добавляет CORS-заголовки). Адрес можно переопределить:

```powershell
flutter run -d edge --dart-define=API_BASE_URL=http://localhost:8787/api/v2
```

## Android

```powershell
flutter run -d <device>
```

На мобильных приложение обращается к API напрямую
(`https://msapi.top-academy.ru/api/v2`), прокси не нужен.

## Устройство проекта

- `lib/core/api_client.dart` — HTTP-клиент, выбор адреса API для web/нативных платформ;
- `lib/core/schedule_database*.dart` — хранилище расписания (sqflite на мобильных, localStorage в браузере);
- `lib/core/notification_service.dart` — локальные уведомления (только на мобильных);
- `tool/api_proxy.dart` — прокси для API + раздача `build/web`.

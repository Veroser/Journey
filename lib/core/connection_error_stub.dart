/// Web-версия: сетевые ошибки браузера приходят как http.ClientException,
/// поэтому отдельных проверок не требуется.
bool isConnectionError(Object error) => false;

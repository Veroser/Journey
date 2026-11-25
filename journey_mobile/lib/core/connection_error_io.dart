import 'dart:io';

bool isConnectionError(Object error) => error is SocketException;

import socket
import logging

HOST = ''
PORT = 32005

logging.basicConfig(
    filename='message_receiver.log',
    level=logging.INFO,
    format='%(asctime)s - %(levelname)s - %(message)s'
)

# Создаём сокет и запускаем сервер
with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as s:
    s.bind((HOST, PORT))
    s.listen()
    logging.info(f'Listening on port {PORT}...')
    while True:
        conn, addr = s.accept()
        with conn:
            logging.info(f'Connected by {addr}')
            
            while True:
                data = conn.recv(1024)
                if not data:
                    break
                message = data.decode('utf-8')
                logging.info(f"Received: {message}")

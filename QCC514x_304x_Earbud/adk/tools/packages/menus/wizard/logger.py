import logging

logger = logging.getLogger('wizard')
logger.setLevel(logging.DEBUG)

console_handler = logging.StreamHandler()
console_handler.setLevel(logging.ERROR)

formatter = logging.Formatter('%(asctime)s - %(name)s - %(levelname)s - %(message)s')
console_handler.setFormatter(formatter)

logger.addHandler(console_handler)


def log_to_file(file_path):
    file_handler = logging.FileHandler(file_path, mode='w')
    file_handler.setLevel(logging.DEBUG)
    logger.addHandler(file_handler)
    file_handler.setFormatter(formatter)

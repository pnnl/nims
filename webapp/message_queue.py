#!/usr/bin/python
import sys
import logging
import threading
from posix_ipc import MessageQueue, SharedMemory, unlink_message_queue
from posix_ipc import ExistentialError


class message_queue:
    O_CREAT = threading.O_CREAT
    O_RDONLY = threading.O_RDONLY

    def __init__(self, mq_name, mode=O_CREAT):
        log_format = '%(asctime)-15s : %(levelname)s : line %(lineno)-4d in %(module)s.%(funcName)s   > %(message)s'
        logging.basicConfig(level=logging.DEBUG, format=log_format)
        self.logger = logging.getLogger('mq-mq_name')

        self.name = mq_name
        self.lock = threading.Lock()
        self.valid = False
        self.delete_on_destruct = False

        if mode is message_queue.O_CREAT:
            self.create_mq()

        else:  # RD_ONLY but need verify for RDWR and WR_ONLY
            self.connect_mq()

        return

    def create_mq(self):
        self.delete_on_destruct = True
        self.logger.info('Attempting to create messsage_queue: ' + self.name)
        try:
            self.mq = MessageQueue(self.name, MessageQueue.O_CREAT)
            self.valid = True

        except:
            self.mq = None
            e = sys.exc_info()[0]
            self.logger.info('Error creating message_queue:' + e.__repr__())
            return None

    def connect_mq(self):
        self.delete_on_destruct = False
        self.logger.info('Attemping to connect to message_queue:' + self.name)
        try:
            MessageQueue(self.name, threading.O_RDONLY)
            self.valid = True
        except ExistentialError as e:
            self.logger.info(' - Could not connect to message_queue::' + e.__repr__())
            self.mq = None

    def send(self, message):
        if not self.valid:
            self.logger.info('Cannot send message:' + self.name + 'is invalid.')
        try:
            self.mq.send(message)
        except:
            e = sys.exc_info()[0]
            self.logger.info('Error sending message:' + e.__repr__())

    def receive(self, timeout=0):
        if not self.valid:
            self.logger.info('Cannot receive messages:' + self.name + 'is invalid.')
            return

        try:
            message = self.mq.receive(timeout)
            return message
        except:
            e = sys.exc_info()[0]
            self.logger.info('Error receiving message:' + e.__repr__())
            return None

    def delete_when_finished(self, delete_when_finished):
        # probably should test type for bool_type
        self.delete_on_destruct = delete_when_finished

    def __enter__(self):
        return

    def __exit__(self):
        if not self.delete_on_destruct
            return

        try:
            self.logger.info('Unlinking MessageQueue:' + self.name)
            unlink_message_queue(self.name)
        except ExistentialError as e:
            pass

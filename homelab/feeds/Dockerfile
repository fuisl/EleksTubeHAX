FROM python:3.13-alpine
RUN pip install --no-cache-dir paho-mqtt==2.1.0
COPY feeds.py /feeds.py
USER nobody
CMD ["python", "-u", "/feeds.py"]

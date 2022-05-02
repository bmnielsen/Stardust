FROM trzeci/emscripten:sdk-tag-1.38.13-64bit as builder

COPY openbw /openbw
COPY cherryvis/build_js.sh /openbw/build_js.sh

WORKDIR /openbw

RUN ./build_js.sh
	
FROM python:3-alpine

COPY cherryvis/requirements.txt .

RUN apk add --no-cache --virtual .build-deps gcc musl-dev \
    && pip install -r requirements.txt \
	&& apk del .build-deps \
	&& rm -rf requirements.txt

COPY cherryvis/src .
COPY --from=builder /openbw/test.* replays/static/replays/viewer/

EXPOSE 8770

CMD ["./server.py","--host","0.0.0.0:8770"]


import os
import sys
import logging
import multiprocessing

import uvicorn

from receiver import app


def setup_logging():
    # Windows GUI 실행 환경에서는
    # stdout/stderr가 None일 수 있으므로 파일로 연결
    if sys.stdout is None:
        sys.stdout = open(os.devnull, "w", encoding="utf-8")

    if sys.stderr is None:
        sys.stderr = open(os.devnull, "w", encoding="utf-8")


def main():
    setup_logging()

    uvicorn.run(
        app,
        host="0.0.0.0",
        port=8080,
        log_level="info",
        access_log=False,
        log_config=None
    )


if __name__ == "__main__":
    multiprocessing.freeze_support()
    main()

FROM public.ecr.aws/lambda/python:3.12

RUN pip install --no-cache-dir Pillow boto3

COPY app.py ${LAMBDA_TASK_ROOT}

CMD ["app.handler"]

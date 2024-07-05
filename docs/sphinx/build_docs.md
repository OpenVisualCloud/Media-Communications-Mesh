# Build documentation guide

## 1. Prerequisites

```
apt install make python3 python3-pip python3-sphinx
```
```
python -m pip install sphinx_book_theme myst_parser sphinxcontrib.mermaid sphinx-copybutton
```

## 2. Build documentation (html)

```
cd {project_dir}/docs/sphinx
```
```
make html
```

## 3.1 Open built documentation (html)

```
cd {project_dir}/docs/_build/html
```

Open index.html via web browser

## 3.2 Alternative run nginx server

```
docker run -it --rm -d -p 8080:80 --name web -v ./docs/_build/html:/usr/share/nginx/html nginx
```

Open index.html via web browser using `http://<you-ip-addr>:8080/` or using local address `http://127.0.0.1:8080/`

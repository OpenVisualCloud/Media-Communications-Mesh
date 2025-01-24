# Build documentation guide

## 1. Prerequisites (Debian)

Download and install dependencies:

```bash
sudo apt-get update --fix-missing
sudo apt-get install -y \
        --no-install-recommends -y \
        python3-sphinx \
        python3-pip \
        python3 \
        make
```

Download and install python3 pip dependencies:

```bash
python3 -m pip install        \
        sphinx_book_theme     \
        myst_parser           \
        sphinxcontrib.mermaid \
        sphinx-copybutton
```

## 2. Build HTML documentation

Execute make build command to build HTML option

```bash
make -C {project_dir}/docs/sphinx html
```

## 3. Open created HTML documentation

```bash
cd {project_dir}/docs/_build/html
```

Open index.html via web browser

### 4. Publish with nginx server

> Note: This step is optional.

```bash
docker run -it --rm -d -p 8080:80 --name web -v ./docs/_build/html:/usr/share/nginx/html nginx
```

Open index.html via web browser using `http://<you-ip-addr>:8080/` or using local address `http://127.0.0.1:8080/`

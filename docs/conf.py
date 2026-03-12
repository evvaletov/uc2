project = "UC2"
copyright = "2026, Eremey Valetov"
author = "Eremey Valetov"
release = "3.0.0"

extensions = [
    "sphinx.ext.autodoc",
    "sphinx.ext.intersphinx",
    "sphinx.ext.githubpages",
]

templates_path = ["_templates"]
exclude_patterns = ["_build"]

html_theme = "furo"
html_static_path = ["_static"]
html_title = "UC2 — UltraCompressor II"
html_logo = None
html_favicon = None

html_theme_options = {
    "source_repository": "https://github.com/evvaletov/uc2",
    "source_branch": "main",
    "source_directory": "docs/",
}

intersphinx_mapping = {
    "python": ("https://docs.python.org/3", None),
}

#!/usr/bin/env bash
# -----------------------------------------------------------------------------
# step5_python.sh
# Set up a Python virtual environment inside the project folder with
# the packages needed for image processing and visualization.
# -----------------------------------------------------------------------------

step5_python() {
    info "Step 5/5 — Setting up Python virtual environment..."

    cd "$HOME/$PROJECT_TITLE"

    if [ -d "$VENV_DIR" ]; then
        warn "Python venv already exists — skipping."
    else
        python3 -m venv "$VENV_DIR"
        ./"$VENV_DIR"/bin/python -m pip install --upgrade pip --quiet
        ./"$VENV_DIR"/bin/python -m pip install $PYTHON_PACKAGES --quiet
        success "Python venv ready. Packages installed: $PYTHON_PACKAGES"
    fi

    # Install numpy globally so tools run without activating the venv
    pip3 install numpy matplotlib --break-system-packages --quiet
}
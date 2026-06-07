#!/usr/bin/env bash
# -----------------------------------------------------------------------------
# step5_python.sh
# Set up a Python virtual environment inside the project folder with
# the packages needed for image processing and visualization.
# -----------------------------------------------------------------------------

step5_python() {
    info "Step 5/5 — Setting up Python virtual environment..."

    cd "$HOME/$PROJECT_TITLE"

    # Determine the host Python command
    if command -v python3 >/dev/null 2>&1; then
        PYTHON_CMD="python3"
    elif command -v python >/dev/null 2>&1; then
        PYTHON_CMD="python"
    else
        error "Python interpreter not found."
    fi

    echo "Using Python interpreter: $PYTHON_CMD"

    if [ -d "$VENV_DIR" ]; then
        warn "Python venv already exists — skipping creation."
    else
        $PYTHON_CMD -m venv "$VENV_DIR"
        ."$VENV_DIR"/bin/python -m pip install --upgrade pip --quiet
        ."$VENV_DIR"/bin/python -m pip install $PYTHON_PACKAGES --quiet
        success "Python venv ready. Packages installed: $PYTHON_PACKAGES"
    fi

    success "Python environment setup complete."
}
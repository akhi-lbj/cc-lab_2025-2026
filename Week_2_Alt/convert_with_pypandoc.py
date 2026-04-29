import pypandoc
import os
import sys
import subprocess
import traceback

root = os.path.dirname(__file__)
md = os.path.join(root, 'README_cfg.md')
pdf = os.path.join(root, 'README_cfg.pdf')

print('Downloading pandoc binary (if needed)...')
try:
    pypandoc.download_pandoc()
    print('pandoc binary downloaded/available.')
except Exception:
    print('Failed to download pandoc binary:')
    traceback.print_exc()

print('Attempting conversion via pypandoc/pandoc...')
try:
    pypandoc.convert_file(md, 'pdf', outputfile=pdf)
    print('Created PDF:', pdf)
    sys.exit(0)
except Exception:
    print('pypandoc conversion failed:')
    traceback.print_exc()

# Fallback: run local md_to_simple_pdf.py
fallback = os.path.join(root, 'md_to_simple_pdf.py')
if os.path.exists(fallback):
    print('Falling back to internal PDF generator...')
    try:
        subprocess.check_call([sys.executable, fallback])
        if os.path.exists(pdf):
            print('Created PDF via fallback:', pdf)
            sys.exit(0)
        else:
            print('Fallback completed but PDF not found at', pdf)
            sys.exit(2)
    except Exception:
        print('Fallback PDF generation failed:')
        traceback.print_exc()
        sys.exit(3)
else:
    print('No fallback script found at', fallback)
    sys.exit(4)

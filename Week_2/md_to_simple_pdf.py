import io
import os

in_path = os.path.join(os.path.dirname(__file__), 'README_cfg.md')
out_path = os.path.join(os.path.dirname(__file__), 'README_cfg.pdf')

with open(in_path, 'r', encoding='utf-8') as f:
    lines = [ln.rstrip('\n') for ln in f]

# Simple wrap function
def wrap_line(s, width=90):
    words = s.split(' ')
    out = []
    cur = ''
    for w in words:
        if cur == '':
            cur = w
        elif len(cur) + 1 + len(w) <= width:
            cur += ' ' + w
        else:
            out.append(cur)
            cur = w
    if cur:
        out.append(cur)
    return out

wrapped = []
for l in lines:
    if l.strip() == '':
        wrapped.append('')
    else:
        wrapped.extend(wrap_line(l, 90))

# Escape parentheses and backslashes for PDF text
def pdf_escape(s):
    return s.replace('\\', '\\\\').replace('(', '\\(').replace(')', '\\)')

text_lines = [pdf_escape(l) for l in wrapped]

# Build content stream: place text at (50, 750) and move down by 14 for each line
content_lines = []
content_lines.append('BT')
content_lines.append('/F1 10 Tf')
content_lines.append('50 750 Td')
for i, tl in enumerate(text_lines):
    # Use Tj for each line and move down 14 units for next
    content_lines.append('(%s) Tj' % tl)
    if i != len(text_lines) - 1:
        content_lines.append('0 -14 Td')
content_lines.append('ET')

content = '\n'.join(content_lines).encode('utf-8')

# PDF objects
objs = []
# 1: Catalog
objs.append(b'1 0 obj\n<< /Type /Catalog /Pages 2 0 R >>\nendobj\n')
# 2: Pages
objs.append(b'2 0 obj\n<< /Type /Pages /Kids [3 0 R] /Count 1 >>\nendobj\n')
# 3: Page
page_obj = b'3 0 obj\n<< /Type /Page /Parent 2 0 R /MediaBox [0 0 612 792] /Contents 4 0 R /Resources << /Font << /F1 5 0 R >> >> >>\nendobj\n'
objs.append(page_obj)
# 4: Contents (stream)
stream = b'stream\n' + content + b'\nendstream\n'
objs.append(b'4 0 obj\n<< /Length %d >>\n' % len(content) + stream + b'endobj\n')
# 5: Font
objs.append(b'5 0 obj\n<< /Type /Font /Subtype /Type1 /BaseFont /Helvetica >>\nendobj\n')

# Write file and compute xref
buf = io.BytesIO()
buf.write(b'%PDF-1.4\n')
offsets = []
for o in objs:
    offsets.append(buf.tell())
    buf.write(o)

startxref = buf.tell()
buf.write(b'xref\n')
buf.write(b'0 %d\n' % (len(objs) + 1))
buf.write(b'0000000000 65535 f\n')
for off in offsets:
    buf.write(b'%010d 00000 n\n' % off)

trailer = b'trailer\n<< /Size %d /Root 1 0 R >>\nstartxref\n%d\n%%%%EOF\n' % (len(objs) + 1, startxref)
buf.write(trailer)

with open(out_path, 'wb') as f:
    f.write(buf.getvalue())

print('Created', out_path)
import io
import os

in_path = os.path.join(os.path.dirname(__file__), 'README_cfg.md')
out_path = os.path.join(os.path.dirname(__file__), 'README_cfg.pdf')

with open(in_path, 'r', encoding='utf-8') as f:
    lines = [ln.rstrip('\n') for ln in f]

# Simple wrap function
def wrap_line(s, width=90):
    words = s.split(' ')
    out = []
    cur = ''
    for w in words:
        if cur == '':
            cur = w
        elif len(cur) + 1 + len(w) <= width:
            cur += ' ' + w
        else:
            out.append(cur)
            cur = w
    if cur:
        out.append(cur)
    return out

wrapped = []
for l in lines:
    if l.strip() == '':
        wrapped.append('')
    else:
        wrapped.extend(wrap_line(l, 90))

# Escape parentheses and backslashes for PDF text
def pdf_escape(s):
    return s.replace('\\', '\\\\').replace('(', '\\(').replace(')', '\\)')

text_lines = [pdf_escape(l) for l in wrapped]

# Build content stream: place text at (50, 750) and move down by 14 for each line
content_lines = []
content_lines.append('BT')
content_lines.append('/F1 10 Tf')
content_lines.append('50 750 Td')
for i, tl in enumerate(text_lines):
    # Use Tj for each line and move down 14 units for next
    content_lines.append('(%s) Tj' % tl)
    if i != len(text_lines) - 1:
        content_lines.append('0 -14 Td')
content_lines.append('ET')

content = '\n'.join(content_lines).encode('utf-8')

# PDF objects
objs = []
# 1: Catalog
objs.append(b'1 0 obj\n<< /Type /Catalog /Pages 2 0 R >>\nendobj\n')
# 2: Pages
objs.append(b'2 0 obj\n<< /Type /Pages /Kids [3 0 R] /Count 1 >>\nendobj\n')
# 3: Page
page_obj = b'3 0 obj\n<< /Type /Page /Parent 2 0 R /MediaBox [0 0 612 792] /Contents 4 0 R /Resources << /Font << /F1 5 0 R >> >> >>\nendobj\n'
objs.append(page_obj)
# 4: Contents (stream)
stream = b'stream\n' + content + b'\nendstream\n'
objs.append(b'4 0 obj\n<< /Length %d >>\n' % len(content) + stream + b'endobj\n')
# 5: Font
objs.append(b'5 0 obj\n<< /Type /Font /Subtype /Type1 /BaseFont /Helvetica >>\nendobj\n')

# Write file and compute xref
buf = io.BytesIO()
buf.write(b'%PDF-1.4\n')
offsets = []
for o in objs:
    offsets.append(buf.tell())
    buf.write(o)

startxref = buf.tell()
buf.write(b'xref\n')
buf.write(b'0 %d\n' % (len(objs) + 1))
buf.write(b'0000000000 65535 f\n')
for off in offsets:
    buf.write(b'%010d 00000 n\n' % off)

trailer = b'trailer\n<< /Size %d /Root 1 0 R >>\nstartxref\n%d\n%%%%EOF\n' % (len(objs) + 1, startxref)
buf.write(trailer)

with open(out_path, 'wb') as f:
    f.write(buf.getvalue())

print('Created', out_path)

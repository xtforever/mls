#!/usr/bin/env python3
import re
import sys
import os

def parse_file(file_path):
    with open(file_path, 'r', encoding='utf-8') as f:
        content = f.read()

    # Regex to find Doxygen-style comments and the following function signature
    # Refined to capture multiline signatures up to the opening brace or semicolon
    pattern = re.compile(r'/\*\*\s*(.*?)\s*\*/\s*\n([\w\s\*]+?)\s+(\w+)\s*\((.*?)\)\s*(?:\{|;)', re.DOTALL)
    
    matches = pattern.finditer(content)
    docs = []

    for match in matches:
        comment = match.group(1)
        return_type = match.group(2).strip()
        func_name = match.group(3).strip()
        params_raw = match.group(4).strip()

        # Clean up comment: remove leading '*' and whitespace from each line
        lines = [line.strip().lstrip('*').strip() for line in comment.split('\n')]
        
        description = []
        params = []
        returns = ""

        for line in lines:
            if line.startswith('@param'):
                params.append(line[6:].strip())
            elif line.startswith('@return'):
                returns = line[7:].strip()
            else:
                if line or description:
                    description.append(line)

        docs.append({
            'name': func_name,
            'signature': f"{return_type} {func_name}({params_raw})",
            'description': "\n".join(description).strip(),
            'params': params,
            'returns': returns
        })

    return docs

def generate_markdown(all_docs):
    md = "# Project API Documentation\n\n"
    
    for filename, functions in all_docs.items():
        md += f"## File: `{filename}`\n\n"
        for func in functions:
            md += f"### `{func['name']}`\n\n"
            md += f"**Signature:**\n```c\n{func['signature']}\n```\n\n"
            if func['description']:
                md += f"{func['description']}\n\n"
            
            if func['params']:
                md += "**Parameters:**\n"
                for param in func['params']:
                    md += f"- {param}\n"
                md += "\n"
            
            if func['returns']:
                md += f"**Returns:** {func['returns']}\n\n"
            
            md += "---\n\n"
            
    return md

def generate_html(all_docs):
    html = """<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>API Documentation</title>
    <style>
        body { font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", Roboto, Helvetica, Arial, sans-serif; line-height: 1.6; color: #333; max-width: 900px; margin: 0 auto; padding: 2rem; background: #fdfdfd; }
        h1 { border-bottom: 2px solid #eee; padding-bottom: 0.5rem; color: #222; }
        h2 { margin-top: 3rem; border-bottom: 1px solid #eee; padding-bottom: 0.3rem; color: #444; background: #f8f9fa; padding: 0.5rem; border-radius: 4px; }
        h3 { margin-top: 2rem; color: #0366d6; font-family: "SFMono-Regular", Consolas, "Liberation Mono", Menlo, monospace; }
        code { font-family: "SFMono-Regular", Consolas, "Liberation Mono", Menlo, monospace; background: #f6f8fa; padding: 0.2rem 0.4rem; border-radius: 3px; font-size: 85%; }
        pre { background: #f6f8fa; padding: 1rem; border-radius: 6px; overflow-x: auto; border: 1px solid #e1e4e8; }
        pre code { background: none; padding: 0; font-size: 100%; color: #24292e; }
        .params { margin: 1rem 0; }
        .params ul { padding-left: 1.5rem; }
        .returns { font-weight: bold; margin-top: 1rem; }
        hr { border: 0; border-top: 1px solid #eee; margin: 2rem 0; }
        nav { position: fixed; right: 2rem; top: 2rem; width: 200px; max-height: 80vh; overflow-y: auto; background: white; padding: 1rem; border: 1px solid #ddd; border-radius: 8px; font-size: 0.8rem; display: none; }
        @media (min-width: 1300px) { nav { display: block; } }
    </style>
</head>
<body>
    <h1>Project API Documentation</h1>
"""
    # TOC (Table of Contents)
    html += "<nav><strong>Contents</strong><ul>"
    for filename in all_docs:
        html += f'<li><a href="#{filename}">{filename}</a></li>'
    html += "</ul></nav>"

    for filename, functions in all_docs.items():
        html += f'<h2 id="{filename}">File: <code>{filename}</code></h2>\n'
        for func in functions:
            html += f'<section id="{func["name"]}">\n'
            html += f'<h3>{func["name"]}</h3>\n'
            html += f'<pre><code>{func["signature"]}</code></pre>\n'
            if func['description']:
                html += f'<p>{func["description"].replace("\\n", "<br>")}</p>\n'
            
            if func['params']:
                html += '<div class="params"><strong>Parameters:</strong><ul>'
                for param in func['params']:
                    html += f'<li><code>{param}</code></li>'
                html += '</ul></div>\n'
            
            if func['returns']:
                html += f'<p class="returns">Returns: <code>{func["returns"]}</code></p>\n'
            
            html += '</section><hr>\n'
            
    html += "</body></html>"
    return html

def main():
    source_files = [
        'lib/mls.c',
        'lib/m_tool.c',
        'lib/m_table.c',
        'lib/m_extra.c'
    ]
    
    all_docs = {}
    for file_path in source_files:
        if os.path.exists(file_path):
            print(f"Processing {file_path}...")
            all_docs[os.path.basename(file_path)] = parse_file(file_path)
        else:
            print(f"Warning: {file_path} not found.")

    # Generate Markdown
    markdown_content = generate_markdown(all_docs)
    with open('API.md', 'w', encoding='utf-8') as f:
        f.write(markdown_content)
    print("Documentation generated in API.md")

    # Generate HTML
    html_content = generate_html(all_docs)
    with open('API.html', 'w', encoding='utf-8') as f:
        f.write(html_content)
    print("Documentation generated in API.html")

if __name__ == "__main__":
    main()

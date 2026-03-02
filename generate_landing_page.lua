-- generate_landing_page.lua

local projects = {
    {
        name = "MLS - Interpreted Comfort for C",
        description = "A C library bringing Python-like dynamic data structures, memory safety, and introspection to C development, freeing you from manual memory management.",
        homepage = "mls_home.html", -- Links to the locally generated MLS project page
        image = "https://via.placeholder.com/150/FF0000/FFFFFF?text=MLS" -- Placeholder image
    },
    {
        name = "Project Flexi-Calc",
        description = "A flexible command-line calculator supporting custom functions and unit conversions, built for speed and ease of use.",
        homepage = "https://example.com/flexicalc",
        image = "https://via.placeholder.com/150/0000FF/FFFFFF?text=CALC"
    },
    {
        name = "TinyNet Daemon",
        description = "A minimalist network daemon for IoT devices, offering secure, low-latency communication with minimal resource footprint.",
        homepage = "https://example.com/tinynet",
        image = "https://via.placeholder.com/150/00FF00/FFFFFF?text=IoT"
    },
    {
        name = "My Awesome Text Editor",
        description = "A blazing fast text editor with Vim-like keybindings, written in pure C for maximum performance.",
        homepage = "https://example.com/editor",
        image = "https://via.placeholder.com/150/FFFF00/000000?text=EDITOR"
    },
    -- Add more projects here
}

local html_content = [[<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>My Awesome Projects</title>
    <style>
        body {
            font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", Roboto, Helvetica, Arial, sans-serif, "Apple Color Emoji", "Segoe UI Emoji", "Segoe UI Symbol";
            margin: 0;
            padding: 20px;
            background-color: #f0f2f5;
            color: #333;
            line-height: 1.6;
        }
        .container {
            max-width: 1200px;
            margin: 0 auto;
            text-align: center;
        }
        h1 {
            color: #2c3e50;
            margin-bottom: 40px;
            font-size: 2.5em;
        }
        .cards-grid {
            display: flex;
            flex-wrap: wrap;
            justify-content: center;
            gap: 20px;
        }
        .card {
            background-color: #fff;
            border-radius: 8px;
            box-shadow: 0 4px 12px rgba(0, 0, 0, 0.08);
            padding: 25px;
            width: 100%; /* Default for mobile */
            max-width: 350px; /* Max width for a single card */
            box-sizing: border-box;
            display: flex;
            flex-direction: column;
            align-items: center;
            text-align: center;
            transition: transform 0.2s ease, box-shadow 0.2s ease;
        }
        .card:hover {
            transform: translateY(-5px);
            box-shadow: 0 8px 20px rgba(0, 0, 0, 0.12);
        }
        .card img {
            width: 100px;
            height: 100px;
            border-radius: 50%;
            object-fit: cover;
            margin-bottom: 15px;
            border: 3px solid #e53935; /* Primary color */
        }
        .card h2 {
            color: #e53935; /* Primary color */
            font-size: 1.5em;
            margin-top: 0;
            margin-bottom: 10px;
        }
        .card p {
            font-size: 0.95em;
            color: #555;
            flex-grow: 1; /* Allow description to take available space */
            margin-bottom: 20px;
        }
        .card a {
            display: inline-block;
            background-color: #e53935; /* Primary color */
            color: #fff;
            padding: 10px 20px;
            border-radius: 5px;
            text-decoration: none;
            font-weight: bold;
            transition: background-color 0.2s ease;
        }
        .card a:hover {
            background-color: #d32f2f; /* Darker shade */
        }

        /* Responsive adjustments */
        @media (min-width: 600px) {
            .card {
                width: calc(50% - 30px); /* Two cards per row */
            }
        }
        @media (min-width: 900px) {
            .card {
                width: calc(33.333% - 27px); /* Three cards per row */
            }
        }
        @media (min-width: 1200px) {
            .card {
                width: calc(25% - 25px); /* Four cards per row */
            }
        }
    </style>
</head>
<body>
    <div class="container">
        <h1>Welcome to My Project Showcase!</h1>
        <div class="cards-grid">
]]

for _, project in ipairs(projects) do
    html_content = html_content .. string.format([[
            <div class="card">
                <img src="%s" alt="%s Logo">
                <h2>%s</h2>
                <p>%s</p>
                <a href="%s" target="_blank">Visit Project Page</a>
            </div>
]], project.image, project.name, project.name, project.description, project.homepage)
end

html_content = html_content .. [[
        </div>
    </div>
</body>
</html>]]

local file = io.open("landing_page.html", "w")
if file then
    file:write(html_content)
    io.close(file)
    print("Generated landing_page.html successfully!")
else
    print("Error: Could not open landing_page.html for writing.")
end

-- CorkScrew Runtime Test ting

local passed = 0
local failed = 0
local total = 0

local function test(name, callback)
    total += 1

    local success, result = pcall(callback)

    if success and result ~= false then
        passed += 1
        print("[PASS] " .. name)
    else
        failed += 1
        warn("[FAIL] " .. name .. " | " .. tostring(result))
    end
end

print("================================")
print("       LUAU RUNTIME TEST")
print("================================")

-- Data types
test("String type", function()
    return type("Hello") == "string"
end)

test("Number type", function()
    return type(123) == "number"
end)

test("Boolean type", function()
    return type(true) == "boolean"
end)

test("Nil type", function()
    return type(nil) == "nil"
end)

test("Function type", function()
    return type(function() end) == "function"
end)

-- Arithmetic
test("Addition", function()
    return 10 + 5 == 15
end)

test("Subtraction", function()
    return 10 - 5 == 5
end)

test("Multiplication", function()
    return 10 * 5 == 50
end)

test("Division", function()
    return 10 / 2 == 5
end)

test("Modulo", function()
    return 10 % 3 == 1
end)

test("Exponentiation", function()
    return 2 ^ 3 == 8
end)

test("Integer division", function()
    return 10 // 3 == 3
end)

-- Functions
test("Function execution", function()
    local function add(a, b)
        return a + b
    end

    return add(10, 20) == 30
end)

test("Multiple returns", function()
    local function getValues()
        return 10, 20, 30
    end

    local a, b, c = getValues()
    return a == 10 and b == 20 and c == 30
end)

test("Variadic arguments", function()
    local function sum(...)
        local total = 0
        for _, v in {...} do
            total += v
        end
        return total
    end

    return sum(1, 2, 3, 4) == 10
end)

-- Tables
test("Table creation", function()
    local t = {1, 2, 3}
    return t[1] == 1 and t[3] == 3
end)

test("Table insertion", function()
    local t = {}
    table.insert(t, "Hello")
    return t[1] == "Hello"
end)

test("Table removal", function()
    local t = {1, 2, 3}
    table.remove(t, 2)
    return #t == 2
end)

test("Table concatenation", function()
    return table.concat({"Lu", "au"}, "") == "Luau"
end)

test("Table iteration", function()
    local t = {a = 1, b = 2}
    local count = 0

    for _ in t do
        count += 1
    end

    return count == 2
end)

-- Luau syntax
test("Compound assignment", function()
    local x = 10
    x += 5
    x *= 2
    return x == 30
end)

test("If expression", function()
    local x = if true then 100 else 200
    return x == 100
end)

test("Type annotations", function()
    local x: number = 42
    local y: string = "test"
    return x == 42 and y == "test"
end)

test("String interpolation", function()
    local name = "Luau"
    return `Hello {name}` == "Hello Luau"
end)

test("Generalized iteration", function()
    local sum = 0

    for _, v in {10, 20, 30} do
        sum += v
    end

    return sum == 60
end)

-- Strings
test("String length", function()
    return #"Luau" == 4
end)

test("String formatting", function()
    return string.format("%s %d", "Test", 123) == "Test 123"
end)

test("String matching", function()
    return string.match("Hello Luau", "Luau") == "Luau"
end)

test("String substitution", function()
    local result = string.gsub("Hello World", "World", "Luau")
    return result == "Hello Luau"
end)

-- Metatables
test("Metatable __index", function()
    local t = setmetatable({}, {
        __index = function()
            return 123
        end
    })

    return t.unknown == 123
end)

test("Metatable __add", function()
    local mt = {
        __add = function(a, b)
            return a.value + b.value
        end
    }

    local a = setmetatable({value = 10}, mt)
    local b = setmetatable({value = 20}, mt)

    return a + b == 30
end)

-- Coroutines
test("Coroutine creation", function()
    local co = coroutine.create(function()
        return 42
    end)

    return type(co) == "thread"
end)

test("Coroutine execution", function()
    local co = coroutine.create(function()
        return 42
    end)

    local success, result = coroutine.resume(co)
    return success and result == 42
end)

test("Coroutine yielding", function()
    local co = coroutine.create(function()
        coroutine.yield("paused")
        return "finished"
    end)

    local ok1, a = coroutine.resume(co)
    local ok2, b = coroutine.resume(co)

    return ok1 and ok2 and a == "paused" and b == "finished"
end)

-- Error handling
test("pcall", function()
    local success = pcall(function()
        error("Test error")
    end)

    return success == false
end)

test("xpcall", function()
    local success = xpcall(function()
        error("Test error")
    end, function(err)
        return tostring(err)
    end)

    return success == false
end)

-- Math
test("Math floor", function()
    return math.floor(4.9) == 4
end)

test("Math ceil", function()
    return math.ceil(4.1) == 5
end)

test("Math random range", function()
    local n = math.random(1, 100)
    return n >= 1 and n <= 100
end)

test("Math sqrt", function()
    return math.sqrt(144) == 12
end)

-- Performance
test("100K iteration benchmark", function()
    local start = os.clock()
    local sum = 0

    for i = 1, 100000 do
        sum += i
    end

    local elapsed = os.clock() - start

    print(string.format(
        "  Execution time: %.6f seconds",
        elapsed
    ))

    return sum == 5000050000
end)

-- Final report
print("================================")
print("           RESULTS")
print("================================")

print("Passed: " .. passed)
print("Failed: " .. failed)
print("Total:  " .. total)

print(string.format(
    "Success rate: %.2f%%",
    (passed / total) * 100
))

if failed == 0 then
    print("ALL TESTS PASSED!")
else
    warn("SOME TESTS FAILED!")
end

print("================================")
  

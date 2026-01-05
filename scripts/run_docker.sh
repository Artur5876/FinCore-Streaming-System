#!/bin/bash

cd docker
docker-compose up -d

echo ""
echo "Services running:"
echo "  • PostgreSQL: localhost:5432"
echo "  • Redis: localhost:6379"
echo "  • Dev container: docker-compose exec dev bash"
echo ""
echo "To enter dev container: docker-compose exec dev bash"
